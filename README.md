# loxOverride - an implementation of Crafting Interpreters Lox with extra features.

## Challenges
### Chapter 14
- [] run-length encoding for line numbers (overkill?) // maybe use macro for ensuring capacity? - to be done after everything
### Chapter 15
- [] make the stack dynamic
- [x] optimize binary operators - check if it makes a difference later
### Chapter 16
- [x] block comments
### Chapter 17
- [x] ternary operator - later actaully emit bytecode for it
- [x] question mark, colon tokens
### Chapter 18
### Chapter 19
- [x] flexible array members for string obj - use for later objects
- [x] concat strings plus other numbers (numbers)



Lox’s approach to error-handling is rather . . . spare. All errors are fatal and immediately halt the interpreter. There’s no way for user code to recover from an error. If Lox were a real language, this is one of the first things I would remedy.
