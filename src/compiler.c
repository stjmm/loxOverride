#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "chunk.h"
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

typedef struct {
    local_t locals[UINT8_COUNT];
    int local_count; // How many locals are in scope
    int scope_depth; // Current scope depth
} compiler_t;

parser_t parser;
compiler_t *current = NULL;
chunk_t *compiling_chunk;

static chunk_t *current_chunk(void)
{
    return compiling_chunk;
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

static void emit_return(void)
{
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

static void init_compiler(compiler_t *compiler)
{
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    current = compiler;
}

static void end_compiler(void)
{
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        dissasemble_chunk(current_chunk(), "code");
    }
#endif
}

static void begin_scope(void)
{
    current->scope_depth++;
}

static void end_scope(void)
{
    while (current->local_count > 0 &&
           current->locals[current->local_count - 1].depth > current->scope_depth) {
        emit_byte(OP_POP);
        current->scope_depth--;
    }
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
static parse_rule_t *get_rule(token_type_e type);
static void parse_precedence(precedence_e precedence);
static int identifier_constant(token_t *name);
static int resolve_local(compiler_t *compiler, token_t *name);
static int parse_variable(const char *error_message);
static void define_variable(int global);

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

static void declaration(void)
{
    if (match(TOKEN_VAR)) {
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
    [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
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
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
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

static int parse_variable(const char *error_message)
{
    consume(TOKEN_IDENTIFIER, error_message);

    declare_variable();
    if (current->scope_depth > 0) return 0;

    return identifier_constant(&parser.previous);
}

bool compile(const char *source, chunk_t *chunk)
{
    init_scanner(source);
    compiler_t compiler;
    init_compiler(&compiler);
    compiling_chunk = chunk;
    parser.panic_mode = false;
    parser.had_error = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    end_compiler();

    return !parser.had_error;
}
