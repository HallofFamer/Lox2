# Lox2
A superset of Lox with ByteCode Interpreter, with multi-pass compiler, optional type checker and tons of new features.


## Introduction
Lox2 is an extended version of the programming language Lox in C. It has new features such as namespace system, exception handling, async/await and optional static typing. The future versions of Lox2 will continue to add more useful features, with improvement of the type system as top priority.

The original version of Lox programming language can be found at Bob Nystrom's repository:
https://github.com/munificent/craftinginterpreters


## Features

### Original Features from Lox
- Stacked based bytecode VM with the basic OPCode support.
- On-demand Scanner, Pratt Parser and Single-Pass Compiler.
- Uniform runtime representation for Lox Value. 
- Basic Unary and Binary Expression/Operators.
- Support for global and local variables.
- If..Else and Switch condition Statement.
- While Loop, Break and Continue Statement.
- Functions and Closures with automatic upvalue capture.
- Mark and sweep Garbage Collector.
- Classes, Objects, Methods and `this` keyword.
- Single Inheritance and `super` keyword.
- Performance improvement with Faster hash table probing and Nan-Boxing.

### New Features in Lox2
- Framework for creating native functions, methods and classes.
- Array/Dictionary Literals and square bracket notation for array/dictionary access.
- New Operators: Modulo(`%`), Range(`..`) and Nil Handling(`?.`, `??`, `?:`).
- Operator overloading to allow operators to be treated as method calls, thus can be used by user defined classes.
- Variadic Functions, Anonymous Functions(local return) and Lambda Expressions(nonlocal return).
- `Object` root class for every class in Lox, everything is an object, and every object has a class.
- Object ID and generic object map which enable inheritance for special build-in classes such as `String` and `Array`.
- Class methods in class declaration using `class` keyword, which are actually instance methods defined on metaclasses.
- Trait declaration using trait keyword, as well as implementing trait via with keyword.
- Anonymous classes/traits similar to anonymous functions/lambda.
- Namespace as Lox2's module system, allowing importing namespace and aliasing of imported classes, traits and functions.
- Lox2 Standard Library for packages `lang`, `util`, `collection`, `io` and `net` in bullt-in namespace `clox.std`.
- Raise exception with `throw` keyword, and exception handling with try/catch/finally statement.
- Interceptor methods which are invoked automatically when getting/setting properties, invoking methods or throwing exceptions.
- Generator functions/methods which can yield control back to the caller and resume at a later point of execution.
- Promise API with event loop provided by libuv library for non-blocking IO operations.
- Introduction of `async` and `await` keywords, which allows C#/JS style of concurrency.
- Optional static typing support for function/method parameters and return values, types only exist at compile time and are erased at runtime. 
- Semicolon inference as well as basic type inference for immutable local/global variables. 
- Dedicated syntax for declaring function types and metaclass types, enabling anonymous functions/lambda to be type inferred. 
- Allow declaration of object instance/class fields outside of class initializer, which also enables optional static typing and default values.
- Iterator API for user defined classes, allowing easy implementation of iterable classes which can be used in for loop.
- Customized Runtime configuration for Lox2 using lox2.ini.
- Allow loading lox source files in lox script and another lox source file with `require` keyword.
- Cross-platform build with Cmake and package manager with vcpkg.

### Enhanced or Removed Features
- VM is no longer a global variable, allowing Lox2 VM to be embedded in other host applications.
- Multi-pass compiler with abstract syntax tree, semantic analyzer(resolver), symbol table, type checker, and generation of bytecode by walking AST. 
- Extend parser with infinite lookahead and backtrack, allowing parsing context sensitive grammar for Lox2.
- Print statement removed, use native function `print` and `println` instead.
- Initializer method is renamed from `init` to `__init__` as an interceptor method.
- Separated integer values(`Int`) and floating point(`Float`) values from `Number`.
- Improved string concatenation, addition of string interpolation and UTF-8 strings.
- C style For Loop replaced with Python/Kotlin style for-in Loop.
- Introduction of keyword `extends` instead of using `<` operator for class inheritance.
- Global variables are scoped within the file it is declared, effectively becoming module variable.
- Function/Method parameters become immutable by default, but may be mutable with `var` keyword.
- Built-in and user defined classes/functions become immutable, and cannot be accidentally overwritten.
- Fix reentrancy problem with Lox2, calling Lox2 closures in C API becomes possible.
- Most runtime errors in VM interpreter loop replaced with Exceptions that can be caught at runtime.
- Inline caching for VM optimization, as well as implementation of Shape(Hidden Class) for better instance variable representation.
- Upgraded the mark and sweep GC with a generational GC which has multiple regions for objects of various generations, which leads to GC running faster when marking/freeing objects. 


## Documentation
[Lox2's documentation](https://mysidia-inc.gitbook.io/lox2) can be found on gitbook, which explores the features in Lox2 as well as some concrete example Lox2 programs. It is still a work in progress as the standard library reference and VM internals sections are still under construction. 

At this moment only the web version is available, though in Lox2 v2.2.0 onwards, Lox2 may have PDF documentation to download for interested developers. 


## Roadmap

Below are the features planned for future versions of Lox2, the list is subject to change but it gives a basic idea of the future directions of this project. 

For a list of implemented features, please see [CHANGELOG.md](https://github.com/HallofFamer/Lox2/blob/master/notes/CHANGELOG.md).

### Lox2 v2.2.0
- Enhanced type system with basic support for generics/parametric polymorphism.
- `type` keyword used as declaration of type alias, useful for complex generic types.
- Capability of saving bytecode into disk as .loxo file, which can be loaded later for faster compilation.
- Refactor classes in the existing standard library to use generics(in `clox.std` parent package), such as `Array<T>` and `Promise<T>`.

### Lox2 v2.3.0
- Additional type system enhancement for union types, with `|` operator on types such as `String | Number`.
- Type ascription using `as` keyword, allowing userland to specify the type of an expression rather than inferred by compiler.
- Trailing closure similar to Kotlin and Swift which allows last lambda argument to be placed outside of parenthesis.
- Add new namespace `clox.ext` which may be optionally enabled/disabled in lox2.ini, add new package `clox.ext.text` for text processing.

### Lox2 v2.4.0
- Support for structural pattern matching using `match` keyword, remove `switch` statement as it has been superceded.
- Enhanced Generics/parametric polymorphism with constraints on generic type parameters using `extends` or `with` keywords.
- Improved type system with non-nullable by default for type declaration, as well as variance for method parameter/return types.
- Foreign function interface(FFI) as a way to write Lox2 libraries in C and load in lox script.

### Lox2 v2.5.0
- C# style property accessor syntax which provides a flexible way to access and modify the value of instance fields. 
- Support for "this type" in method declarations using `this` or `this class`, which allows returning the same type as the class in which methods are declared. 
- Implement a profiler which can identify the "Hotspots" of the program and how long they execute, prerequiste for future JIT. 
- Add new package `clox.ext.sql` which supports database connections and queries for mysql/mariadb, postgres, sqlite, etc.

### Lox2 v2.6.0
- Initial implementation of tracing JIT compiler that optimizes hot loops, only for x64 architecture. 
- Object literal syntax similar to Javascript which can be good for configuration objects. 
- Refine `if` and `match` as expressions, with the value produced being the last expression/statement of the expression body.
- Add Lox2 CLI to run Lox scripts easily from command line, backed by libuv. 

### Lox2 v2.7.0
- Add control flow graph (CFG) and data flow analysis in the compiler, which can be used for constant propagation, dead code elimination and other optimizations.
- Improvement of JIT compiler with further optimizations made possible from Lox2's new CFG and data  flow analysis.
- First class continuation with keyword `thisContext`, enabling manipulation of call stack in userland.
- Add new package `clox.ext.gui` which supports writing GUI applications using gtk binding for Lox2.


## Example
The following code shows how to send a simple asynchronous HTTP request and prints its status and contents to the console. This example makes uses of various features from Lox2 such as namespace declaration/import, immutable variable, async/await, and string interpolation. 

```
namespace Lox2Example
using clox.std.net.HTTPClient
using clox.std.net.URL

val client = HTTPClient()
val response = await client.getAsync(URL.parse("https://example.com"))
println("Response Status: ${response.status}")
println("Content: ${response.content}")
```


## Build and Run Lox2

#### Windows(with git, cmake and vcpkg, need to replace [$VCPKG_PATH] with installation path of vcpkg)
```
git clone -b v2.1.2 https://github.com/HallofFamer/Lox2.git
cd Lox2
cmake -DCMAKE_TOOLCHAIN_FILE:STRING="[$VCPKG_PATH]/scripts/buildsystems/vcpkg.cmake" -S . -B ./build
cmake --build ./build --config Release
./x64/Release/Lox2
```

#### Linux(with git, cmake, curl and libuv, need to install one of the libcurl4-dev and libuv1.dev packages)
```
git clone -b v2.1.2 https://github.com/HallofFamer/Lox2.git
cd Lox2
mkdir build
cmake -S . -B ./build
cmake --build ./build --config Release
./build/Lox2
```

#### Docker(linux, need to replace [$LinuxDockerfile] by actual docker file name, ie. UbuntuDockerfile)
```
git clone -b v2.1.2 https://github.com/HallofFamer/Lox2.git
cd Lox2
docker build -t lox2:linux -f Docker/[$LinuxDockerfile] .
docker run -w /Lox2-2.1.2/Lox2 -i -t lox2:linux
```

Note: It is recommended to clone from the latest stable release branch("v2.1.2" at this moment), as the master branch receives updates regularly and some changes may break existing code. 


## Credits & Special Thanks

#### Below is the attribution list for my Lox2's design and implementation, please contact me if anything else is missing. This does not include 3rd-party libraries whose copyrights are already present in the header files.

- [Robert Nystrom](https://github.com/munificent/craftinginterpreters): For the original Lox language specification and source code that this project is based on. 
- [Smalltalk-lang](https://squeak.org/): For the inspiration of object and class model.
- [Ruby-lang](https://www.ruby-lang.org/en/): For the inspiration of generic ID map/table which allows inheritance and properties on any objects.
- [Javascript-lang](https://www.javascript.com/): For the inspiration of object shape/hidden-class.
- [Wren-lang](https://wren.io/): For the inspiration of UTF-8 string implementation as well as dynamic array macros.
- [Michael Malone(cometlang)](https://cometlang.com/): For the inspiration of exception and try/catch/finally statement. 
- [RevengerWizard(teascript)](https://github.com/RevengerWizard/teascript): For the inspiration of reentrancy in Lox language. 

## FAQ

#### What is the purpose of this project?
Lox2 is a superset of the original Lox language with multi-pass bytecode VM. It serves as an education and experimental language, yet it is meant to be production ready instead of being a toy language. 

#### What are the reasons behind the design of Lox2?
Please see [DESIGN.md](https://github.com/HallofFamer/Lox2/blob/master/notes/DESIGN.md) note for more information, it provides a detailed explanation on the choice of new features being implemented, as well as insights into the evolution of Lox2 as a general purpose programming language.

#### Is there a roadmap for Lox2's type system enhancement?
Please see [TYPESYSTEM.md](https://github.com/HallofFamer/Lox2/blob/master/notes/TYPESYSTEM.md) note for more information, the type system is planned to be enhanced in the next few releases, though the feature list may be subject to change.

#### Can I use the code of Lox2 as base for my own project?
This project is open source and the codebase can be used as base for someone else's project. It has an MIT license, and attribution must be given except for code from the original Lox implementation or third party libraries.