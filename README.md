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
