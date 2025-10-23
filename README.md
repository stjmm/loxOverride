# Chapter 1
Setting up CMAKE: to build use `cmake -S. -Bbuild`, `cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release`, `cmake --build build`

We have chunks which hold the bytecode, line for each OP, and inside the `code` we also have indices of constatns `value_array_t` array. value module is about constants, and each entry in value array is a constant.
Because our bytecode is 8bit int we can have only 256 constants - we remedy this by adding OP_CONSTANT_LONG.

## Challenges
- [] run length encoding - overkill?
- [x] OP_CONSTANT_LONG - use 24 bits for constants - a LOT of constants available


# Chapter 2
We use vm goes through a chunks bytecode and moves thourhg it with `ip`. It also uses stack for `value_t` and pushes/pops when needed for a specific bytecode OP.

## Challenges
- [x] optimize bytecode operations for the least push/pops from the stack


# Chapter 3
Our scanner holds `const char *start` which points to the start of current lexeme, and `current` which is current char in this lexeme. The scanner scans tokens one at a time.
So the compiler goes through the tokens one by one producing bytecode in a chunk. The chunk is held by vm and is interpreted.
Our parser can produce keyword tokens, numbers, strings. Each token holds it's whole lexeme and type, by having start char pointer and length.
`scanner.start` only moves after scanning a whole token.

## Challenges
- [x] block comments


# Chapter 4
Pratt parser starts by the infix operator and calling it. Then we do infix operator parses the infix until the operator we call precedence is lower or equal than the original function precedence. So for PREC_ASSIGNMENT that would be PREC_NONE or PREC_ASSIGNMENT.
Or PREC_UNARY, only PREC_CALL (.) would be parsed in infix loop.

## Challenges
- [] ternary operator


# Chapter 5
So each `value_t` now has a type, and a union `as`. We change our compiler a little bit to emit a specific value with type, and we add operators for them. Tokens like BANG_EQUAL actaully will emit bytecode like OP_EQUAL, OP_NOT.


# Chapter 6
`obj_string_t` has `obj_t` as its first member. We can then cast `obj_string_t` to `obj_t` so our `value_t` has a pointer to the first element of the string -> so the string as well.
Right now my concat number + string implementation allocates a `obj_strin_t` on a converted number.

## Challenges
- [x] flexible array member (chars in obj_string_t)
- [x] concat strings, numbers and both

# Chapter 7
We use a hash map with linear probing. When we delete a entry, we want to set it to a tombstone, so we can find further entries, or return the first tombstone for setting for example.
It uses it to intern ALL strings.

# Chapter 8
Declaration - binds a new name to a value
Statements - control flow, print etc.
Declarations aren't allowed in control flow statements.
statement      → exprStmt
               | printStmt ;

declaration    → varDecl
               | statement ;
We match and consume declarations, which may be many.
To declare a variable we the identifier to constant table, eval the expression, which will get added on stack, and then add to the stack OP_DEFINE_GLOBAL and one or two bytes for idx. In vm we read it as a string (key) and set it the firsts stack val (value) to the globals hash table. For getting/setting we do a similar things in that we add the identifier as a new constant, and do OP_GET/SET and constant idx.

# Chapter 9

We emit clauses like OP_JUMP, or OP_JUMP_IF_FALSE or OP_LOOP, that either just jump, jump if evaluated false or jump back. Jumps can be 16bit.

## Challenges
- [x] ternary
- [x] break/continue
- [x] switch, case, default
add: modulus, ++, --

# Chapter 26
Roots - objects that the VM can reach directly
We want to go through object and mark the current object black and objects the black refrences gray until no more gray. Then we sweep the white ones.

# Chapter 28
## Challenges
- [x] challenge 1
