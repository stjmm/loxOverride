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
#include "native.h"

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
}

static void runtime_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--) {
        call_frame_t *frame = &vm.frames[i];
        obj_function_t *function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack();
} 

static void define_native(const char *name, native_fn function)
{
    push(OBJ_VAL(allocate_string(name, (int)strlen(name))));
    push(OBJ_VAL(new_native(function)));
    table_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void init_vm(void)
{
    reset_stack();
    vm.objects = NULL;
    init_table(&vm.globals);
    init_table(&vm.strings);

    define_native("clock", clock_native);
    define_native("input", input_native);
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

static bool call(obj_closure_t *closure, int arg_count)
{
    if (arg_count != closure->function->arity) {
        runtime_error("Expected %d arguments but got %d.",
                      closure->function->arity, arg_count);
        return false;
    }

    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }

    call_frame_t *frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

static bool call_value(value_t calle, int arg_count)
{
    if (IS_OBJ(calle)) {
        switch (OBJ_TYPE(calle)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(calle), arg_count);
            case OBJ_NATIVE: {
                native_fn native = AS_NATIVE(calle);
                value_t result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            default:
                break;
        }
    }
    runtime_error("Can only call functions and classes.");
    return false;
}

static obj_upvalue_t *capture_upvalue(value_t *local)
{
    obj_upvalue_t *created_upvalue = new_upvalue(local);
    return created_upvalue;
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
    call_frame_t *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_TWO_BYTES() ((uint16_t)(READ_BYTE() | (READ_BYTE() << 8)))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_16() (frame->closure->function->chunk.constants.values[READ_TWO_BYTES()]) 
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
        dissasemble_instruction(&frame->closure->function->chunk,
                    (int)(frame->ip - frame->closure->function->chunk.code));
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
                value_t constant = frame->closure->function->chunk.constants.values[offset];
                push(constant);
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
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
            case OP_DUP: push(peek(0)); break;
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
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
                obj_string_t *name = READ_STRING_16();
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
            case OP_JUMP: {
                uint16_t offset = READ_TWO_BYTES();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_TWO_BYTES();
                if (is_falsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_TWO_BYTES();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                int arg_count = READ_BYTE();
                if (!call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_CLOSURE: {
                obj_function_t *function = AS_FUNCTION(READ_CONSTANT());
                obj_closure_t *closure = new_closure(function);
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalue_count; i++) {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_RETURN: {
                value_t result = pop();
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
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
    obj_function_t *function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    obj_closure_t *closure = new_closure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}
