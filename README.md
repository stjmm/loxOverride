# loxOverride

An implementation of **Clox** bytecode interpreter from [*Crafting Interpreters*](https://craftinginterpreters.com/) book by Robert Nystorm.
Built following the book, with several extra features from the challenges and some my own.

## Challenges & New Features

Here's a list of new features I've implemented:
- JavaScript-like Dynamic arrays
    Implemented with hash tables.
    ```
    var array = [1, 2, 3, 4];
    array[50] = 50;
    ```

- Break/Continue (Chapter 23, Challenge 1,2)
    Loop control flow statements.
    ```
    while(true) {
        if (x == 5) break;
        if (x == 6) continue;
    }
    ```

- Switch/Case/Default (Chapter 23, Challenge 1,2)
    C-style switch statements.
    ```
    switch(x) {
        case 1:
            print x;
            break;
        default:
            break;
    }
    ```

- Ternary operator (Chapter 17, Challenge 3)
    ```
    var happy = true ? "happy" : "sad";
    ```

- Block Comments

    ```
    /* Multi line
        block comment */
    print "after comment";
    ```

- Concatenating strings + numbers (Chapter 19, Challenge 3)
    Adding strings and numbers produce a string.
    ```
    var x = "Nr." + 42;
    print x; //-> Nr. 42
    ```


## Other challenges implemented that are not exactly features
- Chapter 15, Challenge 2
- Chapter 15, Challenge 4
- Chapter 19, Challenge 1
- Chapter 28, Challenge 1


## Building & Running

### Prerequesites
- **C Compiler**
- **CMake**

### 1. Clone the repository
```bash
$ git clone https://github.com/stjmm/loxOverride
$ cd loxOverride
```

### 2. Configure CMake 
By default the project will build in **Debug** mode.
```bash
$ cmake -S. -Bbuild
```
To build in **Release** mode.
```bash
$ cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release
```

### 3. Build and run
```bash
$ cmake --build build
$ ./build/clox
$ ./build/clox someprogram.lox
```
