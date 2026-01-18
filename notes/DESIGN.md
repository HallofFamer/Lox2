# Design
This document explains the design decisions behind the implemented features of Lox2. 

## Why everything is an object? 
Besides the fact that Lox2 is an OO language, having everything as an object adds a level of consistency. In lesser OO languages such as C++ and Java, programmers have to be aware of the difference between objects and primitive values, and some features work only for objects or primitive values, while this is unnecessary in better OO languages such as Smalltalk and Ruby.

## Why is Lox2 standard library implemented in C? 
Lox2 is not a fully statically typed language like C++ or Java, even with optional typing it is performance is still nowhere near compiled languages. Implementing standard library in C guarantees faster native functions/methods, also there are certain features that cannot be implemented in Lox2 itself, such as IO. 

## What are non-local returns and how are they useful? 
Lox2's lambda expressions use non-local returns, this allows return from the lambda's enclosing function/method instead of just the lambda itself. Non-local return is a valuable feature that allows exiting a function/method that cannot be done otherwise, a feature I've missed a lot when using Javascript's anonymous/arrow functions.

## Why choose class methods over static methods?
Although they seem identical at first glance, there is a subtle difference between class/static methods. Class methods are actually methods defined on the metaclass, while static methods are just namespaced functions. Class methods use dynamic dispatch and can be polymorphic, they are much more powerful than static methods, and fit better with Lox's object model.

## Why Java/C# like namespaces instead of Javascript/Python like module system? 
A standard module system will work very well for Lox2, but the idea is that namespace system does not tie the source code with file/directory structure. In future, there is a chance that AOT compilation will be possible with Lox2, and the flexibility provided by the namespace system will come in handy.

## How are the implementation of Shape and Inline Cache improve performance?
Lox2's object fields are implemented as hash tables, and looking up an instance variable has performance hit. With the shape and inline cache, it is possible to access instance variable via index/offset, which is faster than hash table lookup. Unfortunately without JIT, Lox2 cannot make best use of the inline cache, but the performance improvement is still noticeable.

## What is generic object map and how is it useful?
The generic object map stores a list of special objects(not represented by ObjInstance) and their fields, to allow accessing fields such as `length` on String/Array objects, and `superclass` on Class objects. This allows these special objects to continue to use a more efficient internal representation than ObjInstance, while allowing access to its fields as standard objects.

## Why async/await instead of fiber or green thread?
I am well aware of the controversy surrounding async/await as well as its shortcomings. However, implementing async/await is a valuable learning experience(you need knowledge of generator and promise to pull it off), especially considering how poorly this topic is covered in most compiler/interpreter books. It also integrates very well with the popular libuv library. 

## What are the reasons to switch to the `extends` keyword?
The original Lox2 implementation uses `<` sign, but trait are implemented using `with` keyword, which feels a little odd and inconsistent(Robert Nystrom did mention he would not want to introduce another keyword near the end of the book). More importantly, this decision simplifies future work on parsing Generics which will use angle bracket syntax.

## Why use type before variable name notation in type declaration?
Lox2's type declaration syntax is inspired by C/C++/Java/C#/Dart. Although languages like Typescript and Kotlin use variable name before type notation, I believe type before variable name notation is more readable and familiar to people coming from mainstream languages. On the other hand, I'd love a little challenge implementing a parser that supports infinite look-ahead and backtrace.

## Why use angle bracket syntax (<>) for generics?
Angle bracket syntax is widely used in mainstream languages such as C++, Java, C#, Dart, Kotlin, Typescript and Rust. It is actually quite easy to read for humans, despite making the parser quite a bit more complex. As classes/types are first class objects in Lox2, the square bracket syntax won't make it much easier for the parser as it cannot disambiguate between array indexing and generic type parameterization easily anyway.

## How does Lox2's reified generics work?
Lox2's generics are reified, meaning that generic type information is available at runtime. This is achieved by passing type arguments as hidden parameters to generic functions/methods so they may be accessed as normal parameters. Generic classes, on the other hand, store type arguments as hidden instance fields. This allows Lox2 to use and inspect generic type information at runtime without using reflection.

## What does partial reification mean in Lox2's generics?
Lox2's generics are partially reified at this moment, as type parameters are reified as class or trait objects without additional type information for higher order types. For example, if `T` is `List<String>`, it will be be reified to `List` class object as runtime, and it is not possible to tell that `T` is a list of strings instead of a list of integers. This is a compromise to reduce runtime overhead.