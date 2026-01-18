# Omitted Features

This document lists features that are intentionally omitted from Lox2 language design, along with explanations for each decision. These omitted features are based on careful consideration of language simplicity, usability, maintainability, and the overall philosophy of Lox2. They will not be considered for future versions of Lox2 unless there is a compelling reason to revisit these decisions.

## Increment and Decrement Operators
The increment (`++`) and decrement (`--`) operators are not available in Lox2. This decision was made to maintain clarity and simplicity in the language. These operators can lead to confusion regarding their side effects, especially in complex expressions. Instead, Lox2 encourages the use of explicit addition and subtraction for better readability. Many modern languages are moving away from these operators for similar reasons.

## Bitwise Operators
Bitwise operators (such as `&`, `|`, `^`, `~`, `<<`, `>>`) are not included in Lox2. The primary reason for this omission is that bitwise operations are less commonly used in high-level programming and can complicate the language syntax. Bitwise operations can often be replaced with more intuitive arithmetic or logical operations. Omitting the right shift (`>>`) operators also make it slightly easier to parse nested generics.

## Ternary Conditional Operator
Lox2 does not support the ternary conditional operator (`condition ? expr1 : expr2`). While this operator can provide a concise way to express conditional logic, it can also reduce code readability, especially for those unfamiliar with its syntax. On the other hand, Lox2 plans to support if-else expressions in future, which offer similar functionality with improved readability.

## Do-While Loops
The do-while loop construct is not included in Lox2, as it does not offer significant advantages over the existing while loop. The while loop is sufficient for most use cases, and removing the do-while construct simplifies the language syntax. This decision aligns with the goal of keeping Lox2 straightforward and encourage one obvious way to perform looping.

## C style for Loops
The original Lox toy language used to support C style for loops, but it has been modified into a for-in loop in Lox2. This change was made to simplify the loop construct and make it more intuitive for iterating over collections. The for-in loop is easier to read and reduces the likelihood of errors associated with traditional C style loops, such as off-by-one errors.

## Abstract classes and methods
Abstract classes and methods are usually supported in statically typed object-oriented languages such as Java and C#. Lox2's object model, on the other hand, closely resembles that of Smalltalk, which does not have the concept of abstract classes/methods. Lox2 also encourages using traits which are more flexible and composable than abstract classes, a decision shared by Swift.

## Visibility Modifiers
Lox2 does not include visibility modifiers (like `public`, `private`, `protected`), as seen in languages like Java and C#. This omission is intentional to keep the language simple and focused on core functionality. Instead, Lox2 relies on conventions and module boundaries to manage access control, promoting a more straightforward approach to encapsulation.

## Structural Typing
Lox2 employs nominal typing rather than structural typing. Structural typing may be more flexible, but it can lead to undesirable behaviors when two irrelevant types share same structure. Nominal typing also aligns better with Lox2's object model, while structural typing can make type checking complex and slow at the presence of higher order types.

## Local variable type annotations
It is not possible to declare local variable types in Lox2. This decision was made to encourages using immutable local variables with type inference. The Java community still struggle to move past explicit local variable type annotations, as many refuse to embrase the new `var` keyword in favor of redundant explicit type annotations which become noises/distractions. Lox2 aims to promote cleaner and more maintainable code by avoiding unnecessary verbosity.

## Type declaration for lambda expressions
Even though type annotations are available for anonymous functions, the lambda expressions cannot be explicitly typed. This decision was made to keep lambda expressions concise and focused on their functionality rather than type details. On the other hand, Lox2's lambda parameters are enclosed in pipe symbols (`|`), which will conflict with union type syntax to be introduced in Lox 2.3.

## Generic Covariance and Contravariance
Some languages support covariance and contravariance in generics, allowing for more flexible type relationships. Lox2 does not support userland covariance and contravariance in generics as they introduce a level of complexity that even senior developers often struggle to understand and apply correctly, while contravariance is especially confusing and rarely useful in practical applications. In future versions of Lox2 though, function/method return types may support covariance.

## Higher Kinded Types
Lox2 will not support higher-kinded types. This feature, while powerful, adds significant complexity to the type system. I believe that there is a tradeoff between expressiveness and simplicity, and Lox2 tries to find the sweet spot in between, while higher-kinded types tend to push the language towards the more complex side of the spectrum with minimal practical benefits for everyday programming tasks.

## Dependent Types
Dependent types, which allow types to depend on values, will not be included in Lox2. While dependent types can enhance type safety and expressiveness, they also introduce significant complexity to the type system and more importantly, for average developers to use them properly. There is always a point where too much type level programming becomes counterproductive, and dependent types do not offer significant practical benefits for the added complexity they bring.