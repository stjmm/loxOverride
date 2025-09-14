#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "chunk.h"
#include "scanner.h"
#include "value.h"
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

typedef void (*parse_fn)(void);

typedef struct {
    parse_fn prefix;
    parse_fn infix;
    precedence_e precedence;
} parse_rule_t;

parser_t parser;
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

static void emit_constant(value_t value)
{
    if(!write_constant(current_chunk(), value, parser.previous.line)) {
        error("Too many constants in one chunk. 16-bit max.");
    }
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


static parse_rule_t *get_rule(token_type_e type);
static void parse_precedence(precedence_e precedence);

static void expression(void)
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void grouping(void)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(void)
{
    double value = strtod(parser.previous.start, NULL);
    emit_constant(value);
}

static void unary(void)
{
    token_type_e operator_type = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return;
    }
}

static void binary(void)
{
    token_type_e operator_type = parser.previous.type;
    parse_rule_t *rule = get_rule(operator_type);
    parse_precedence((precedence_e)(rule->precedence + 1));

    switch (operator_type) {
        case TOKEN_PLUS:  emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:  emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
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
    [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
    [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
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
    prefix_rule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        parse_fn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule();
    }
}

bool compile(const char *source, chunk_t *chunk)
{
    init_scanner(source);
    parser.panic_mode = false;
    parser.had_error = false;
    compiling_chunk = chunk;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    end_compiler();

    return !parser.had_error;
}
