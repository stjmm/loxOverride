#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "vm.h"
#include "chunk.h"
#include "debug.h"
#include "compiler.h"
#include "table.h"
#include "value.h"
#include "object.h"
#include "memory.h"

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
}

static void runtime_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
}

void init_vm(void)
{
    reset_stack();
    vm.objects = NULL;
    init_table(&vm.globals);
    init_table(&vm.strings);
}

void free_vm(void)
{
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

void push(value_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t pop(void)
{
    vm.stack_top--;
    return *vm.stack_top;
}

static value_t peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool is_falsey(value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static obj_string_t *concatenate(value_t a_val, value_t b_val)
{
    obj_string_t *a = IS_STRING(a_val) ? AS_STRING(a_val) : number_to_string(AS_NUMBER(a_val));
    obj_string_t *b = IS_STRING(b_val) ? AS_STRING(b_val) : number_to_string(AS_NUMBER(b_val));
    return concatenate_strings(a, b);
}

static interpret_result_e run(void)
{
#define READ_BYTE() (*vm.ip++) // Reads byte/instruction and advances
#define READ_TWO_BYTES() ((uint16_t)(READ_BYTE() | (READ_BYTE() << 8)))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // Reads index from bytecode and advances
#define READ_CONSTANT_16() (vm.chunk->constants.values[READ_TWO_BYTES()]) 
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_STRING_16() AS_STRING(READ_CONSTANT_16())
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        vm.stack_top[-2] = value_type(AS_NUMBER(vm.stack_top[-2]) op AS_NUMBER(vm.stack_top[-1])); \
        vm.stack_top--; \
       } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("        ");
        for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[");
            print_value(*slot);
            printf("]");
        }
        printf("\n");
        dissasemble_instruction(vm.chunk,
                                (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                value_t constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_CONSTANT_16: {
                uint16_t offset = READ_TWO_BYTES();
                value_t constant = vm.chunk->constants.values[offset];
                push(constant);
                break;
            }
            case OP_EQUAL: {
                vm.stack_top[-2] = BOOL_VAL(values_equal(vm.stack_top[-2], vm.stack_top[-1]));
                vm.stack_top--;
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                value_t b_val = vm.stack_top[-1];
                value_t a_val = vm.stack_top[-2];
                if (IS_NUMBER(a_val) && IS_NUMBER(b_val)) {
                    BINARY_OP(NUMBER_VAL, +);
                } else if ((IS_STRING(a_val) || IS_NUMBER(a_val)) &&
                    (IS_STRING(b_val) || IS_NUMBER(b_val))) {
                    obj_string_t *result = concatenate(a_val, b_val);
                    vm.stack_top[-2] = OBJ_VAL(result);
                    vm.stack_top--;
                } else {
                    runtime_error("Operands must be numbers or strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm.stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm.stack[slot] = peek(0);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                obj_string_t *name = READ_STRING();
                table_set(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_DEFINE_GLOBAL_16: {
                obj_string_t *name = READ_STRING_16();
                table_set(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL: {
                obj_string_t *name = READ_STRING();
                value_t value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_GET_GLOBAL_16: {
                obj_string_t *name = READ_STRING_16();
                value_t value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL: {
                obj_string_t *name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                }
                break;
            }
            case OP_SET_GLOBAL_16: {
                obj_string_t *name = READ_STRING();
                if (table_set(&vm.globals, name, peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                }
                break;
            }
            case OP_NOT:
                push(BOOL_VAL(is_falsey(pop())));
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                AS_NUMBER(vm.stack_top[-1]) = -AS_NUMBER(vm.stack_top[-1]);
                break;
            case OP_PRINT: {
                print_value(pop());
                printf("\n");
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_TWO_BYTES
#undef READ_CONSTANT
#undef READ_CONSTANT_16
#undef READ_STRING
#undef READ_STRING_16
#undef BINARY_OP
}

interpret_result_e interpret(const char *source)
{
    chunk_t chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    interpret_result_e result = run();

    free_chunk(&chunk);
    return result;
}
