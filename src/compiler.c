#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "scanner.h"
#include "value.h"
#include "object.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    token_t previous;
    token_t current;
    bool had_error;
    bool panic_mode;
} parser_t;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_TERNARY,
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} precedence_e;

typedef void (*parse_fn)(bool can_assign);

typedef struct {
    parse_fn prefix;
    parse_fn infix;
    precedence_e precedence;
} parse_rule_t;

typedef struct {
    token_t name;
    int depth; // Its depth
} local_t;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT,
} function_type_e;

typedef struct {
    int loop_start;
    int break_jumps[32];
    int break_count;
    int scope_depth;
    bool is_switch;
} control_context_t;

typedef struct compiler_t {
    struct compiler_t *enclosing;
    obj_function_t *function;
    function_type_e type;

    local_t locals[UINT8_COUNT];
    int local_count;
    int scope_depth;
    
    control_context_t control_stack[16]; // TODO: Check break jumps and control_stack count
    int control_stack_top;
} compiler_t;

parser_t parser;
compiler_t *current = NULL;

static chunk_t *current_chunk(void)
{
    return &current->function->chunk;
}
static control_context_t *current_context(void)
{
    return &current->control_stack[current->control_stack_top];
}

static void error_at(token_t *token, const char *message)
{
    if (parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {

    } else {
        fprintf(stderr, " at '%.*s'\n", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

// parser.previous error
static void error(const char *message)
{
    error_at(&parser.previous, message);
}

// parser.current error
static void error_at_current(const char *message)
{
    error_at(&parser.current, message);
}

static void advance(void)
{
    parser.previous = parser.current;

    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break; 

        error_at_current(parser.current.start);
    }
}

static void consume(token_type_e type, const char *message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }

    error_at_current(message);
}

static bool check(token_type_e type)
{
    return parser.current.type == type;
}

static bool match(token_type_e type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

static void emit_byte(uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte_1, uint8_t byte_2)
{
    write_chunk(current_chunk(), byte_1, parser.previous.line);
    write_chunk(current_chunk(), byte_2, parser.previous.line);
}

static void emit_loop(int loop_start)
{
    emit_byte(OP_LOOP);

    int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emit_byte((offset >> 0) & 0xFF);
    emit_byte((offset >> 8) & 0xFF);
}

static int emit_jump(uint8_t instruction)
{
    emit_byte(instruction);
    // Jump up to 16 bytes
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void emit_return(void)
{
    emit_byte(OP_NIL);
    emit_byte(OP_RETURN);
}

static int make_constant(value_t value)
{
    return add_constant(current_chunk(), value);
}

static void emit_constant(value_t value)
{
    int constant = make_constant(value);

    if (constant <= UINT8_MAX) {
        emit_bytes(OP_CONSTANT, (uint8_t)constant);
    } else if (constant <= UINT16_MAX) {
        emit_byte(OP_CONSTANT_16);
        emit_byte((constant >> 0) & 0xFF);
        emit_byte((constant >> 8) & 0xFF);
    } else {
        error("Too many constants in one chunk. 16-bit max.");
    }
}

static void patch_jump(int offset)
{
    int jump = current_chunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    current_chunk()->code[offset] =     (jump >> 0) & 0xFF;
    current_chunk()->code[offset + 1] = (jump >> 8) & 0xFF;
}

static void init_compiler(compiler_t *compiler, function_type_e type)
{
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->control_stack_top = -1;
    compiler->function = new_function();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = allocate_string(parser.previous.start,
                                                  parser.previous.length);
    }

    local_t *local = &current->locals[current->local_count++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static obj_function_t *end_compiler(void)
{
    emit_return();
    obj_function_t *function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        dissasemble_chunk(current_chunk(), function->name != NULL
                          ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void begin_scope(void)
{
    current->scope_depth++;
}

static void end_scope(void)
{
    current->scope_depth--;

    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > current->scope_depth) {
        emit_byte(OP_POP);
        current->local_count--;
    }
}

void begin_control_context(int start, bool is_switch)
{
    current->control_stack_top++;
    control_context_t *ctx = current_context();
    ctx->is_switch = is_switch;
    ctx->loop_start = start;
    ctx->break_count = 0;
    ctx->scope_depth = current->scope_depth;
}

void end_control_context(void)
{
    control_context_t *ctx = current_context();

    for (int i = 0; i < ctx->break_count; i++) {
        patch_jump(ctx->break_jumps[i]);
    }

    current->control_stack_top--;
}

static void synchronize(void)
{
    parser.panic_mode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
            case TOKEN_SWITCH:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ;
        }

        advance();
    }
}


static void expression(void);
static void declaration(void);
static void statement(void);
static void named_variable(token_t name, bool can_assign);
static parse_rule_t *get_rule(token_type_e type);
static void parse_precedence(precedence_e precedence);
static void mark_initialized(void);
static int identifier_constant(token_t *name);
static int resolve_local(compiler_t *compiler, token_t *name);
static int parse_variable(const char *error_message);
static void define_variable(int global);
static uint8_t argument_list(void);

static void expression(void)
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void block(void)
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(function_type_e type)
{
    compiler_t compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_current("Can't have more than 255 parameters.");
            }
            uint8_t constant = parse_variable("Expect parameter name.");
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    obj_function_t *function = end_compiler();
    emit_bytes(OP_CONSTANT, make_constant(OBJ_VAL(function)));
}

static void fun_declaration(void)
{
    uint8_t global = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void var_declaration(void)
{
    int global = parse_variable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    define_variable(global);
}

static void expression_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void print_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void return_statement(void)
{
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emit_return();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
}

static void for_statement(void)
{
    begin_scope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    // Initializer
    if (match(TOKEN_SEMICOLON)) {
        // No initializer
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    int loop_start = current_chunk()->count;
    begin_control_context(loop_start, false);

    // Condition
    int exit_jump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of loop if condition false.
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    // Parse incrememnt but run it at the end of loop
    if (!match(TOKEN_RIGHT_PAREN)) {
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;
        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
        
        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);

    if (exit_jump != 1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_control_context();
    end_scope();
}

static void if_statement(void)
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(); // if (...) - condition value will be on the stack
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    // Emit then jump (OP_CODE + two bytes for long jumps)
    int then_jump = emit_jump(OP_JUMP_IF_FALSE); // Index where the instruction is
    emit_byte(OP_POP);
    statement();

    int else_jump = emit_jump(OP_JUMP); // Jump if condition was true

    patch_jump(then_jump);
    emit_byte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patch_jump(else_jump);
}

static void while_statement(void)
{
    int loop_start = current_chunk()->count;
    begin_control_context(loop_start, false);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);

    end_control_context();
}

static void switch_statement(void)
{
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    expression(); // Condition on the stack
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    // Switch (x)
    consume(TOKEN_LEFT_BRACE, "Expect '{' after 'switch'");

    int start_switch = current_chunk()->count;
    begin_control_context(start_switch, true);

    bool has_default = false; // Only one default possible
    bool has_case = false;

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        if (match(TOKEN_CASE)) {
            has_case = true;
            emit_byte(OP_DUP); // Dup switch condition
            expression();
            consume(TOKEN_COLON, "Expect ':' after case value.");
            
            emit_byte(OP_EQUAL);
            int case_jump = emit_jump(OP_JUMP_IF_FALSE);
            emit_byte(OP_POP);

            while (!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) &&
                    !check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
                statement();
            }

            patch_jump(case_jump);
            emit_byte(OP_POP);
        } else if (match(TOKEN_DEFAULT)) {
            if (has_default) {
                error("Multiple 'default' labels in one switch.");
            }
            has_default = true;

            consume(TOKEN_COLON, "Expect ':' after default.");

            while (!check(TOKEN_CASE) && !check(TOKEN_DEFAULT) &&
                    !check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
                statement();
            }
        } else {
            error("Expect 'case' or 'default'.");
            synchronize();
        }
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch cases.");
    end_control_context();
    emit_byte(OP_POP); // Pop the dupped value
}

static void break_statement(void)
{
    if (current->control_stack_top < 0) {
        error("Can't use 'break' outside of a loop or switch statement.");
        return;
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");

    control_context_t *ctx = current_context();
    
    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > ctx->scope_depth) {
        emit_byte(OP_POP);
        current->local_count--;
    }

    int jump = emit_jump(OP_JUMP);
    ctx->break_jumps[ctx->break_count++] = jump;
}

static void continue_statement(void)
{
    control_context_t *ctx = current_context();

    if (ctx->is_switch || current->control_stack_top < 0) {
        error("Can't use 'continue' outside of a loop statement.");
        return;
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > ctx->scope_depth) {
        emit_byte(OP_POP);
        current->local_count--;
    }

    emit_loop(ctx->loop_start);
}

static void declaration(void)
{
    if (match(TOKEN_FUN)) {
        fun_declaration();
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

    if (parser.panic_mode) synchronize();
}

static void statement(void)
{
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match(TOKEN_RETURN)){
        return_statement();
    } else if (match(TOKEN_IF)){
        if_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_SWITCH)) {
        switch_statement();
    } else if (match(TOKEN_BREAK)) {
        break_statement();
    } else if (match(TOKEN_CONTINUE)) {
        continue_statement();
    } else {
        expression_statement();
    }
}

static void grouping(bool can_assign)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign)
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static void string(bool can_assign)
{
    emit_constant(OBJ_VAL(allocate_string(parser.previous.start + 1,
                                      parser.previous.length - 2)));
}

static void named_variable(token_t name, bool can_assign)
{
    int arg = resolve_local(current, &name);

    if (arg != -1) {
        // Local variable
        if (can_assign && match(TOKEN_EQUAL)) {
            expression();
            emit_bytes(OP_SET_LOCAL, (uint8_t)arg);
        } else {
            emit_bytes(OP_GET_LOCAL, (uint8_t)arg);
        }
    } else {
        arg = identifier_constant(&name);
        if (arg <= UINT8_MAX) {
            if (can_assign && match(TOKEN_EQUAL)) {
                expression();
                emit_bytes(OP_SET_GLOBAL, (uint8_t)arg);
            } else {
                emit_bytes(OP_GET_GLOBAL, (uint8_t)arg);
            }
        } else if (arg <= UINT16_MAX) {
            if (can_assign && match(TOKEN_EQUAL)) {
                expression();
                emit_byte(OP_SET_GLOBAL_16);
                emit_byte((arg >> 0) & 0xFF);
                emit_byte((arg >> 8) & 0xFF);
            } else {
                emit_byte(OP_GET_GLOBAL_16);
                emit_byte((arg >> 0) & 0xFF);
                emit_byte((arg >> 8) & 0xFF);
            }
        } else {
            error("Too many constants. 16-bit max.");
        }
    }

}

static void variable(bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign)
{
    token_type_e operator_type = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        case TOKEN_BANG:  emit_byte(OP_NOT); break;
        default: return;
    }
}

static void binary(bool can_assign)
{
    token_type_e operator_type = parser.previous.type;
    parse_rule_t *rule = get_rule(operator_type);
    parse_precedence((precedence_e)(rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_PLUS:          emit_byte(OP_ADD); break;
        case TOKEN_MINUS:         emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:          emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:         emit_byte(OP_DIVIDE); break;
        case TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER:       emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:          emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
        default: return;
    }
}

static void call(bool can_assign)
{
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

static void ternary(bool can_assign)
{
    int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);

    parse_precedence(PREC_ASSIGNMENT);

    int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);
    emit_byte(OP_POP);

    consume(TOKEN_COLON, "Expect ':' after then branch of conditional expression.");

    parse_precedence(PREC_ASSIGNMENT);

    patch_jump(else_jump);
}

static void and_(bool can_assign)
{
    // Left side consumed and sits on the stack. If false jump.
    int end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void or_(bool can_assign)
{
    int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static void literal(bool can_assign)
{
    switch (parser.previous.type) {
        case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL:   emit_byte(OP_NIL); break;
        default: return;
    }
}

parse_rule_t rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_QUESTION]      = {NULL,     ternary,PREC_TERNARY},
    [TOKEN_COLON]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static parse_rule_t *get_rule(token_type_e type)
{
    return &rules[type];
}

static void parse_precedence(precedence_e precedence)
{
    advance();
    parse_fn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        parse_fn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static int identifier_constant(token_t *name)
{
    return add_constant(current_chunk(), OBJ_VAL(allocate_string(name->start,
                                                                 name->length)));
}

static bool identifiers_equal(token_t *a, token_t *b)
{
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static void mark_initialized(void)
{
    if (current->scope_depth == 0) return;
    current->locals[current->local_count - 1].depth =
        current->scope_depth;
}

static int resolve_local(compiler_t *compiler, token_t *name)
{
    for (int i = compiler->local_count - 1; i >=0; i--) {
        local_t *local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void add_local(token_t name)
{
    if (current->local_count == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    local_t *local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
}

static void declare_variable(void)
{
    if (current->scope_depth == 0) return;

    token_t *name = &parser.previous;
    for (int i = current->local_count - 1; i >= 0; i--) {
        local_t *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;
        }

        if (identifiers_equal(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    add_local(*name);
}

static void define_variable(int global)
{
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }

    if (global <= UINT8_MAX) {
        emit_bytes(OP_DEFINE_GLOBAL, (uint8_t)global);
    } else if (global <= UINT16_MAX) {
        emit_byte(OP_DEFINE_GLOBAL_16);
        emit_byte((global >> 0) & 0xFF);
        emit_byte((global >> 8) & 0xFF);
    } else {
        error("Too many global variables. 16 bit max.");
    }
}

static uint8_t argument_list(void)
{
    uint8_t arg_count = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (arg_count == 255) {
                error("Can't have more than 255 arguments.");
            }
            arg_count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguemnts.");
    return arg_count;
}

static int parse_variable(const char *error_message)
{
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    if (current->scope_depth > 0) return 0;

    return identifier_constant(&parser.previous);
}

obj_function_t *compile(const char *source)
{
    init_scanner(source);
    compiler_t compiler;
    init_compiler(&compiler, TYPE_SCRIPT);

    parser.panic_mode = false;
    parser.had_error = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    obj_function_t *function = end_compiler();
    return parser.had_error ? NULL : function;
}
