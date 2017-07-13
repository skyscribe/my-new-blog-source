---
layout: post
title: Java8中的函数式编程
categories: [language, programming, fp, design]
tags: [java, programming, language, fp, pattern, design, optional, stream]
---

Java8是日益臃肿、略显老态的老牌程序语言对日益流行的**新的函数式编程范式**的反击；
它采用了巧妙的技术让自己面向对象的古老躯体也套上了函数式编程的舞鞋再度翩翩起舞，焕发新的光彩。

<!--more-->

## FP的基本要素：函数
Java并不打算放弃其面向对象的内核 - 所以的东西必须都是对象，那么函数也不例外，它依然是对象。具体而言，是一个特殊的函数式接口的实现。

### 函数依然是对象
在新的`java.util.function`包里，预定义了形形色色的函数接口,譬如带2个参数的函数

```java
@FunctionalInterface
public interface BiFunction<T, U, R>{
  //all functions implements apply that takes t of type T and u of type U, and returns R
  R apply(T t, U u);

  //compose function
  default <V> BiFunction<T, U, V> andThen(Function<? super R, ? extends V> after) {
    Objects.requrieNotNull(after);
    return (T t, U u) -> after.apply(apply(t, u));
  }

}
```
从定义来看，它是一个接口，任何实现了该接口的对象都是一个二元函数。从纯粹的面向对象思路来看，只要让所有实现类似调用的类也实现新的接口就行。
仔细考虑则发现该思路有个不足 - 很可能我们已经有一个已有的接口

```java
public interface MyInterface{
  ReturnType doSomething(ArgType1 arg1, ArgType2 arg2);
}

public class MyBusinessClassA implements MyInterface {
  @Override
  ReturnType doSomething(ArgType1 arg1, ArgType2 arg2){
    //detailed implementation
  }
}
```

考虑用户代码想要使用上述的接口，同时希望采用函数式编程的思路，接受一个函数参数
```java
//client code
void myMethod(BiFunction<ArgType1, ArgType2, ReturnType> certainProcessing) {
  //calls certainProcessing.apply(arg1, arg2);
}

MyBusinessClassA businessClass;// = ...;
ArgType1 arg1; //= ...;
ArgType2 arg2; //= ...;
myMethod(/*use existing business classes?*/)
```
如果仅仅是因为需要采用函数式编程风格来编写代码，就必须要求我们修改原来的接口或者class定义破坏性就太大了。Java8采用了变通机制，
当**某个接口有且仅有一个方法定义**的时候，该接口可以被编译器隐式地转换为一个函数式接口的一个扩展；实现了此业务接口的类则被视为函数式接口的一个具体实现。
对应的这个方法(`doSomething`)会被认为是默认的`apply`方法，即使这个方法的名字是别的，
只要参数个数、类型、返回类型能匹配到对应的泛型函数接口的对应类型(`doSomething(...)`)，它就会被视为是实际实现的方法。
当用户代码用函数式编程的风格来调用抽象的函数式对象(`BiFunction`)的`apply`方法时，实际调用会被委托为实际实现方法(`doSomething`)的调用。

为方便代码维护和协助编译器检查的考虑，Java8提供了`@FunctionalInterface`注解方便我们清晰的知道某个接口符合函数式接口的要求。
也许是为了更灵活的配合函数式编程，Java8也允许一个接口中有提供默认实现(用`default`关键字标识)，此时这样的方法被认为是具体的而非抽象的；
这样只要一个接口中**有且只有一个抽象的方法**，它依然符合函数对象的要求。

如果接口声明没有加上`@FunctionalInterface`注解，调用的地方却使用了lambda表达式语法，编译器会检查对应的类接口中是否满足函数式对象的约束如有违反则报错。
因而该注解不是必要的；出于代码可读性的考虑，还是建议尽量加上该注解。

### 内置函数

Java8的工具库中提供了一些常见的基于泛型的函数式接口

- 返回值为布尔类型的函数 - Predicate
- 参数类型和返回值类型一样的函数 - `Operator`
- 没有返回值的函数 - `Consumer`
- 不带参数而能产生某些返回的函数 - `Producer`
- 基于非装箱原始类型的特殊函数 - 某些时候希望避免自动装箱和自动解箱的性能开销
- 添加了参数个数信息的函数，譬如`BinaryOperator<T>`用于描述签名为`(T a, T b) -> T`的函数;对应的`UnaryOperator<T>`用于描述的函数签名为`(T a) -> T`

依据以上的命名风格，`java.util.function`中定义了几十个泛型的函数接口，可以满足几乎所有的简单场景。查看[JDK文档](https://docs.oracle.com/javase/8/docs/api/java/util/function/package-summary.html)可以发现, Java8**没有提供超过2个参数**的函数接口定义，默认的`Function<T, R>`描述的是最普通的单映射函数。

### 匿名函数和Lambda

匿名函数其实是被Java8给封装成具体的函数对象（实现某个预定义的接口)。语法上没什么奇特的地方，和C++/Haskell的都比较类似。
在任何一个可以传入函数调用的地方，都可以传入lambda表达式或者代码块，并且类型信息可以省略,如上边代码的例子

```java
MyBusinessClassA businessClass;// = ...;
ArgType1 arg1; //= ...;
ArgType2 arg2; //= ...;
myMethod((arg1, arg2) -> businessClass.doSomething(arg1, arg2));
```

lambda表达式中引用上下文变量的情况下，该lambda表达式自然形成了一个闭包。
当函数实现逻辑不能用一行写下来的时候，也可以用大括号写代码块；和其它主流支持函数式编程的语言没什么两样。

## Optional类型
Optional类型是个典型的容器类型，用来表示有一个合法值或者空值；其本身是一种简单的[Monad类型](http://learnyouahaskell.com/a-fistful-of-monads)。

### 从错误处理方式说起
长久以来，Java都是采用两种方式处理可能的逻辑例外情况，要么是采用返回对象的方式，要么是采用异常传递。
Java的异常在设计上被分类为检查异常和非检查异常；前者别用来表述一些可以恢复的意外情况，编译器会在编译的时候检查可能抛出该类异常的API的**调用方必须显式的处理**检查异常；
非检查异常用来表述编程上的错误；严格来说非检查异常应该是代码的某些地方处理除了问题；需要通过修改代码来解决问题。

Sun是这样设计Java的异常机制的，开源社区却对这种编译器强制检查的异常颇有微词，甚至很多有名的开源项目都**主张永远不使用检查异常**；
遍布`try/catch`块的代码的可读性非常差，可怜的业务逻辑很容易淹没在异常处理代码的包围圈里。仅使用非检查异常的副作用是，程序很容易因为没有正确处理的异常而崩溃。
于是很多程序员会转而采用破坏对象状态的方式，到处传递空指针；为了避免空指针异常导致的程序崩溃，我们有不得不在代码中加上很多引用是否为空的判断，写出越来越难维护的代码。

> I call it my billion-dollar mistake. It was the invention of the null reference in 1965. At that time, I was designing the first comprehensive type system for references in an object oriented language (ALGOL W). My goal was to ensure that all use of references should be absolutely safe, with checking performed automatically by the compiler. But I couldn't resist the temptation to put in a null reference, simply because it was so easy to implement. This has led to innumerable errors, vulnerabilities, and system crashes, which have probably caused a billion dollars of pain and damage in the last forty years.
> -- Tony Honare, apology for inventing the null reference

### 用Optional来处理错误
传统的C/C++语言中，返回值也是一种处理错误的方式；其不足之处是，正常处理和异常情况的处理会产生很多复杂的逻辑判断，导致正常的逻辑难以理清；Java不建议集成这笔古老的遗产。
`Optional`类则提供了**一种新的错误处理方式**；从概念上来说，这三种方式是不能同时采用的，要么采用`Optional`要么采用异常，但是不应该两者都采用。

从概念上来说，一个`Optional`对象是关于某种具体对象的一个容器，它要么包含一个已经初始化的对象，要么什么也没有。
看起来和null引用没什么区别；主要的差异在于类型系统上 - Java是个静态语言，null对象不是一个合法初始化过的对象，对它做任何方法调用都会引起引用异常；
`Optional`则不同，即使没有正确的初始化某个对象，它**本身依然是一个合法的对象**。它用一套统一的接口来操作内部封装的对象。

具体到Java8的定义它本质上是一个不可被外部构造和继承的一个具体的类，可以参考JDK的源码
```java
public final class Optional<T> {
  private static final Optional<?> EMPTY = new Optional<>();
  private final T value;
}
```

从容器封装的角度来看，`Optional`可以**看作是集合类的一个特殊的退化情况** - 它要么保护一个对象，要么什么也没有；但是不能包含超过两个或更多个对象。
`Optional`提供了很多API供我们使用

### 构造出新的Optional对象

构造的方式有很多种，可以用默认的空构造创建一个空的对象，此时内部的封装对象没有被初始化；另外一种方式是静态方法
- `of`方法传入一个**非null**的对象，构造出包含给定对象的容器
- `ofNullable`方法传入一个可能为null的对象；构造中会依据是否为null来决定是创建初始化的容器还是空容器

`Optional`对象一经创建就不允许在修改其内部的状态；但是可以通过`get`方法来获取内部存储的对象 - 如果是空则会抛出`NoSuchElementException`。
一般情况下不建议未经检查便直接调用`get`方法；因为`Optional`本身提供了很多函数式编程的模式。

### 模式

有如下模式可以供我们组合使用

- `filter` 提供过滤器功能，可以依照用户传入的一个谓词函数对容器中的对象进行过滤，如果谓词判断为真，则原封不动返回原容器，如果是假，则返回空容器
- `map` 提供对象转换功能，其参数是一个转换函数，实现一个对象到另外一个对象的转换；
这里`Optional`保证传给参数函数的输入值一定不会是空对象，即**转换函数不需要做null判断**因为map实现本身已经帮你判断了。
从函数式编程的角度来看，`map`是一个高阶函数 - 其参数是另外一个函数
- `flatMap` 和上边的`map`类似，差别在于传入的参数函数的返回这无法确保非null的情况下选择了一个新的`Optional`类型作为返回；
为避免`Optional<Optional<T>>`的麻烦，`flatMap`会将这个二层封装给解开，生成一个单一的封装。
其实现代码非常简单
```java
public<U> Optional<U> map(Function<? super T, ? extends U> mapper) {
  Objects.requireNonNull(mapper);
  if (!isPresent())
    return empty();
  else {
    return Optional.ofNullable(mapper.apply(value));
  }
}
```

- `orElse` 提供容器封装对象的提取；如果原来的容器里存有合法的对象，则直接返回此对象；如果没有则返回参数提供的默认值。
这里的提取实际上是一个解封装操作；返回的对象同样也抱枕是非null的，**拿到这个对象的调用者不需要做额外的null判断**

- `orElseGet` 是一个类似的提取操作，和`orElse`不同的是对于空容器的处理，返回值由一个传入的`Supplier`来提供；同样**也要保证尽量不要提供nulli**
以免让使用者操心null判断的事儿。从函数式编程的角度来看，这也是一个高阶函数。

- `orElseThrow` 则提供了一个和传统的异常相结合的方式，同样不需要外层调用者自己加逻辑判断，容器会在有对象的情况下返回对象出来，
没有则调用传入的`Supplier<? extends Throwable>`抛出一个异常。这同样是一个高阶函数

T.B.D

## Streams API
