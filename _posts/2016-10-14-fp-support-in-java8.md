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
在新的`java.util.function`包里，预定义了形形色色的函数接口，譬如带2个参数的函数的定义如下

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
遍布`try/catch`块的代码的可读性非常差，可怜的业务逻辑很容易淹没在异常处理代码的包围圈里。

仅使用非检查异常的副作用是，程序很容易因为没有正确处理的异常而崩溃。
于是很多程序员会转而采用破坏对象状态的方式，到处传递空指针；为了避免空指针异常导致的程序崩溃，我们有不得不在代码中加上很多引用是否为空的判断，写出越来越难维护的代码。

> I call it my billion-dollar mistake. It was the invention of the null reference in 1965. ... My goal was to ensure that all use of references should be absolutely safe,
> with checking performed automatically by the compiler. But I couldn't resist the temptation to put in a null reference, **simply because it was so easy to implement.** 
> This has led to innumerable errors, vulnerabilities, and system crashes, which have probably caused a billion dollars of pain and damage in the last forty years.
> > -- Tony Honare, apology for inventing the null reference

### 用Optional来处理错误

传统的C/C++语言中，返回值也是一种处理错误的方式；其不足之处是，正常处理和异常情况的处理会产生很多复杂的逻辑判断，导致正常的逻辑难以理清；Java不建议集成这笔古老的遗产。

`Optional`类则提供了**一种新的错误处理方式**；从概念上来说，这三种方式是不能同时采用的，要么采用`Optional`要么采用异常，但是不应该两者都采用。

从概念上来说，一个`Optional`对象是关于某种具体对象的一个容器，它要么包含一个已经初始化的对象，要么什么也没有。
看起来和null引用没什么区别；主要的差异在于类型系统上 - Java是种静态语言，**null对象不是一个合法初始化过的对象**，对它做任何方法调用都会引起引用异常；
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

#### 对封装数据的修改/转换相关的模式

`filter` 提供过滤器功能，可以依照用户传入的一个谓词函数对容器中的对象进行过滤，如果谓词判断为真，则原封不动返回原容器，如果是假，则返回空容器

`map` 提供对象转换功能，其参数是一个转换函数，实现一个对象到另外一个对象的转换；
这里`Optional`保证传给参数函数的输入值一定不会是空对象，即**转换函数不需要做null判断**因为map实现本身已经帮你判断了。
从函数式编程的角度来看，`map`是一个高阶函数 - 其参数是另外一个函数

`flatMap` 和上边的`map`类似，差别在于传入的参数函数的返回这无法确保非null的情况下选择了一个新的`Optional`类型作为返回；
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

#### 数据提取相关的模式
`orElse` 提供容器封装对象的提取；如果原来的容器里存有合法的对象，则直接返回此对象；如果没有则返回参数提供的默认值。
这里的提取实际上是一个解封装操作；返回的对象同样也抱枕是非null的，**拿到这个对象的调用者不需要做额外的null判断**

`orElseGet` 是一个类似的提取操作，和`orElse`不同的是对于空容器的处理，返回值由一个传入的`Supplier`来提供；同样**也要保证尽量不要提供nulli**
以免让使用者操心null判断的事儿。从函数式编程的角度来看，这也是一个高阶函数。

`orElseThrow` 则提供了一个和传统的异常相结合的方式，同样不需要外层调用者自己加逻辑判断，容器会在有对象的情况下返回对象出来，
没有则调用传入的`Supplier<? extends Throwable>`抛出一个异常。这同样是一个高阶函数


上述的模式提供了丰富的组合功能使我们对一个`Optional`对象做函数式编程变得简单明了；甚至不需要一个`if`/`else`分支判断就可以做一些复杂的操作。
比如下面的一段代码
```java
AnotherResult result = Optional.ofNullable(someObj.doSth(parX))
  .filter(someResult -> someResult.meetSomeCondition())
  .map(conditionalResult -> transformAsAnotherResult(conditionalResult))
  //.flatMap(conditionalResult -> transformAsAnotherResult(conditionalResult)) if transformAsAnotherResult returns optional
  .orElseGet(() -> new anotherEmptyResult());
  //.orElseThrow(() -> new SomeRunTimeException()); //if we want to throw

//Now result wouldn't be null at all!
```

#### 反模式
`Optional`可以帮助我们大大简化代码，然而也有一些**反模式需要小心留意**；比如以下这些

##### 违反基本的约束
以下列举了几个常见的基本错误，这些错误只要稍微深入理解下`Optional`的设计思想就可以避免

1. 混用异常和`Optional`类型返回 - 显然两种机制是鱼和熊掌的关系，设计方法的时候必须选择其中一个，而不是两者混用。
如果**选择让方法返回Optional类型，就不要在实现内部再抛出异常**，否则你的用户将会抓狂。

2. 在Optional的值中存放`null` - 这是明显**违背设计契约**的做法，导致`Optional`封装完全失去意义。如果想重新构造一个Optional,
如果不能确保它不是null，请用`ofNullable`

3. 在模式提供的高阶函数的实现中检查参数是否为null - 这里是做了不必要的额外检查，因为`Optional`已经给你保证了传给你的参数不会是`null`。

 譬如下边的实现纯粹是画蛇添足

```java
anOptional.map(v -> doSth(v));

private SomeType doSthn(ValueType v) {
  if (v != null) {
    //do something and generates return type
  } else {
    //This won't be ran!
  }
}
```

##### 冗余的判断
还有一些典型的误用和**不熟悉函数式编程的惯用法**有关，可以通过简单的重构解决

1. 混用`if/else`和Optional的`isPresent()`和`get()` - 这是一种非常常见的误用；往往使得代码变得更加复杂。
因为`Optional`本身就是设计来处理可能的例外情况，更合适的方法是用好上述的模式。

 如果需要提取出值对象，就用`orElse`系列方法；如果不需要产生任何类型的新值，可以用`ifPresent`传入lambda表达式;如果需要将结果从一种类型变化为另外一种，就采用上述的转换模式。

2. 复杂的链式操作，即多个连续的`map`操作 - 这种情况下代码的可读性也变差；根源是不同层次的细节被堆积在一个抽象层次中了；用简单的**重构技巧抽出新的子函数**即可。
逻辑上来说，`anOptional.map(a -> transformAsB(a)).map(b -> transformAsC(b))` 等价于 `anOptional.map(a -> composeTransformAAndB(a))`；这里的字函数都不需要做null判断

3. 用`Optional`类型作函数的参数 - 这个是一个轻微的反模式，IntelliJ IDEA甚至会温馨的提示你需要重构。
原因也比较简单，Optional类型和外部函数组合的时候，都期望通过合适的变换/提取函数将值取出来传出去，是否存在的事儿，用已有的模式去做就可以了。
任何用`Optional`在函数中传递的写法，都**对应一个更简单的复合Optional模式**的写法;为什么不采用这些模式而要自己写判断？

  比如如下的例子
```java
Optional<SomeType> anOptional = ///initialize;
RetType b = doSth(anOptional);

private RetType doSthn(Optional<SomeType> opt){
  return opt.map(obj -> obj.transform()).orElse(new RetType());
}
```

  可以重构为更符合[局部性原理](https://en.wikipedia.org/wiki/Locality_of_reference)的形式,避免`Optional`类型的蔓延
```java
Optional<SomeType> anOptional = ///initialize;
RetType b = anOptional.map(v -> doSthn(v)).orElse(new RetType());

private RetType doSthn(SomeType obj){
  return obj.transform();
}
```
## Streams API
Java8新提供了`Streams` API来实现更类似于Haskell的[List Monad](http://learnyouahaskell.com/a-fistful-of-monads)风格的函数式编程设施；
值得注意的是，在老版本的Java库里边，`List`这个接口已经用来描述传统的基于共享内存模型的数据结构了（和C++的类似）；
这也许是Java8另起炉灶新添加新的接口来描述这一概念。

类似于Functional Interface,Streams API也是包含一系列**新的Java接口**的包的简称；这些接口都放在`java.util.stream`包中。

### 基本概念
Stream是一个函数式编程概念的接口抽象；它和集合类的概念比较类似；比较大的差异在于Stream是
- 关于**操作的抽象**而不是关于数据的抽象，可以将其看作一个流水线，
一些数据流入抽象的Stream,经过某些操作变换产生某些输出；这些输出可能成为流的下一步处理的输入

- 无状态的，所有绑定的**操作不能修改数据源**，即只能决定产生的输出是什么样子，不能回头修改输入的数据；
这也是纯函数式编程所要求的**无副作用**；同样的数据经过某个处理操作产生的输出一定要是一样的。

- **惰性运算**赋值的，即Stream上的操作不一定会消耗所有的输入数据，譬如我们在一个Stream上取前3个数据，
那么即使输入数据有无穷多个，操作也能在取到3个的时候就结束返回给下一步处理。

- 可能有无限多个输入，只要后续的操作是有限的

- 同一个Stream的输入**只能被使用一次**，下一次若想操作必须重新生成Stream；从这点设计约束看，Java的Stream没有Haskell的纯粹;
也可以认为流水线一旦被处理，最原始的数据就不存在了。

### 基本的Stream类型
所有的`Stream`接口都继承自一个公共的**泛型接口** 
```java
public interface BaseStream<T, S extends BaseStream<T, S>> extends AutoClosurable {
  Iterator<T> iterator();
  Iterator<T> spliterator();

  @Override
  void close();
  S onClose(Runnable closeHandler);

  //...other common interfaces...
}
```
其中`T`用于声明其初始输入的元素的类型，`S`则用于将子类的类型带上来，和C++的[CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)技巧类似。
从接口声明上看，一个Stream类也
- 提供了迭代器访问接口，可以用传统的迭代器访问模式操作`Stream`
- 实现了`AutoClosurable`接口；从而我们可以结合Java8的try-with-resource表达式方便的自动管理资源。

  **大部分的Stream实现并不会管理资源**，因而不显示关闭Stream往往也不会带来什么问题。

#### `Stream<T>`接口

最平凡的Stream是名为`Stream<T>`的泛型接口

```java
public interface Stream<T> extends BaseStream<T, Stream<T>> {
  //operations...
  //terminators...
  public interface Builder<T> extends Consumer<T> {
    @Override
    void accept(T t);

    default Builder<T> add(T t){
      accept(t);
      return this;
    }

    Stream<T> build();
  }
}
```

该接口包含一些**转换操作和终止操作**组成；转换操作将Stream中的数据作为输入，经过变换或过滤等产生新的输出；并准备好下一次转换操作或终止流水线；
终止操作则直接终止流水线，返回某些汇聚之后的结果出来。

#### 以Java基本类型为数据元素的特殊Stream
Java泛型技术的一个限制是，对基本的数据类型（这些类型不是一个Object）必须用包装类，
直接用包装类替换基本类型则会带来比较大的[性能开销](https://tavianator.com/java-autoboxing-performance/)；
尤其是Java5之后引入的自动装箱对程序员隐藏了这些实现细节，某种程度上加剧了问题的严重性。

`Stream`对所有的基本类型都提供了一个对应的接口，比如`IntStream`、`LongStream`等等。这些接口都继承自`BasicStream`,暴露的方法和`Stream`比较类似；从接口声明上看，
很多方法和`Stream<T>`都是类似的，仅仅是针对类型做了特殊处理，这个也是很老的一个对Java语言设计的槽点了。

### 生成Stream
大部分情况下，用户不需要手工创建Stream对象；它们可以用不同的方式产生

- 使用`Collection`接口的`.stream()`方法；还有一个对应的`.parallelStream()`返回一个可以并发执行的对象

- 使用`Arrays`工具类的`.stream(Object[])`从一个数组构造出来

- 使用`Stream`本身的一些静态方法产生，包括
 1. `Stream.of(T t)`产生单个元素的Stream对象
 2. `Stream.of(T... values)`从对象列表中产生
 3. `Stream.generate(Supplier<T> s)` 用给定的`Supplier`函数产生无穷序列(其实受`Long`类型的最大可能值限制)
 4. `Stream.concat(Stream<? extends T> a, Stream<? extends T> b)`连接两个已有的流

 这些方法也有对应的针对基本类型的Stream的版本。

- 使用`StreamSupport`辅助类来产生，包括
 1. `stream(Spliterator<T> spliterator, boolean parallel)`从一个迭代器产生，可以支持并发的`Stream`
 2. `stream(Supplier<? extends Spliterator<T>> supplier, int characteristics, boolean parallel)`支持从给定的迭代器的Supplier中一一调用`.get()`方法；支持并发方式迭代
 3. 类似功能的针对基本类型的Stream的封装

- 特定场景的构造方法，比如
 1. 随机数产生器流，用`Random.ints()`
 2. 缓冲的IO流中产生的流 - `BufferedReader.lines()`
 3. 其它形形色色的JDK库提供的封装；以及第三方库提供的封装

- `Builder`接口
该接口是`Stream<T>`的内部接口，扩展了`Consumer<T>`；可以利用`add(T t)`方法添加元素到Stream中；该操作支持链式调用，最后用`build()`方法生成最终的Stream对象。
实现在`Consumer`的接口则具有和`add`类似的语意，只是不支持链式操作。比如`IntStream.Builder.add(1).add(2).add(3).build`生成一个包含3个数字为输入的Stream。

### 流水线操作和变换
作为一种函数式编程工具，Stream就天然是为组合而生的；这些组合本身就构成了流水线处理 - 初始化的元素作为流水线的输入，而中间的转换步骤可以有任意多个，
最终则往往会有一个终止操作来产生期望的输出 - 该输出也是我们从流水线上拿到最终结果的地方。

#### 中间操作和终止操作
所有这些中间操作每次返回一个新的`Stream`状态，其输入是经过转换的 - 如前所述`Stream`的输入是不可修改的；该新的Stream的输入是对前一次的输入，
输出则由前一次操作的针对每一个输入做运算之后的输出组成；如果某些运算不产生输出，则这些数据就像筛子一样被过滤下来了。

所有的终止操作从函数签名上来看，都不会返回新的Stream对象。

所有的中间操作都符合**延迟计算**规则；即真正的输出并没有在调用这些转换操作的时候被计算出来；
只有**当终止操作被调用以取出需要的值的时候**，这些转换操作才真正被计算出来。

当我们提供一个无限长的输入提供给Stream的时候，真正参与具体操作的元素个数仅仅以满足终止条件为准。

譬如下边的代码
```java
int first5PrimeSum = IntStream.iterate(1, i -> i+1)
    .filter(x -> isPrime(x))
    .limit(5)
    .sum();
```

初始构造的Stream包含无穷多个元素（受限于int类型的长度），但是经过`filter`操作之后，由用`limit`取出前5个，并最终求和；
那么实际参与运算的初始输入元素只有1到11而已。

根据以上的赋值求值规则，我们可以认为终止操作是**贪婪求值**的并会产生副作用；一旦调用了终止操作，Stream对象即产生了最终的运算结果；
原始的输入元素遭到了破坏，无法回头重新来过。
例外的情况隐藏在`iterator()`和`splititerator()`操作上，它们虽然是终止操作，却不会破坏流水线的状态，用户可以用`iterator`接口来决定流水线的运算时间点。

#### 中间操作模式
`Stream`接口提供了很多传统的符合函数式编程风格的方法，一些甚至允许更灵活的高阶函数
- `map`用于普通的函数变化，其参数是一个转换函数，将数据从一种类型转换为新的类型
- `flatMap(Function<? super T, ? extend Stream<? extends R>> mapper)`将Stream的每一个元素应用之转换函数，然后将转换结果流中的数据取出来汇聚为新的Stream。
 该操作是一种相对高阶的模式，可以避免手工来拼接流
- `skip`/`limit`分别用于截取或者跳过某些元素
- `sorted`则产生按照给定排序规则排列为有序输出数据的流
- `filter`用来过滤流中满足给定谓词逻辑判断的数据
- `distinct`会删除重复的元素
- `peek`用于在产生输出数据的同时，做一些参数指定的函数操作；该操作大部分时候可以用于方便debug，比如
```java
Stream.of("one", "two", "three")
  .filter(e -> e.lenth() > 3)
  .peek(e -> System.out.println("Filtered value: " + e))
  .map(String::toUpperCase())
  .peek(e -> System.out.println("Mapped value: " + e))
  .collect(Collectors.toList());
```

#### 有状态和无状态的中间操作

中间操作可以携带lambda表达式，从而可以简单将其分类为有状态的和无状态的操作。
有状态的操作会**携带额外的上下文信息**，如果这些操作的运算结果跟操作的中间结果有关，则Stream的行为会变得依赖于流水线的执行顺序 - 如果操作被并发调度，
结果就会显得不确定。

比如这个例子
```python
Set<Integer> seen = Collections.synchronizedSet(new HashSet<>());
stream.parallel().map(e -> if (seen.add(e)) {
      return 0; 
    } else {
      return e;
    })
  .reduction(...)
```
第一个`map`操作时间的执行体中会根据给定的元素是否已经处理过来决定返回0或元素本身；这个lambda操作本身依赖于之前的运算所以是有状态的。

显然有状态的操作在用`parallelStream`调度计算的时候**会产生不确定结果**；而无状态的操作则没有这样的副作用。

Java8引入`Stream`的首要目的就是并行计算和并发，默认串行的操作，仅仅需要在生成`Stream`的地方加上`parallel()`就可以自动获得多核并发调度的好处。

#### 潜在的操作干扰

传给Stream的数据在Stream运算的过程中被视为静态的；如果这些数据可能被同时修改，则操作的正确性就难以保证了。
考虑一个用Java的ArrayList产生的Stream做运算的情况

```java
List<String> strList = new ArrayList(Arrays.asList("one", "two", "three");
Stream<String> strm = strList.stream();
strList.add("four"); //WoW!
String result = strm.collect(joining(" "));
```
有状态的函数操作或者互相干扰的函数操作会**破坏并发安全性并带来出乎意料的行为**（因为多线程、同步、调度的细节被隐藏了）；应该不建议使用。

#### 常用的终止操作
终止操作的方法很多，基本上`Stream`的API中，除了静态方法以外，所有的返回类型不是`Stream`的都是终止操作，常用的有
- `allMatch`/`anyMatch/nonMatch`接受一个谓词函数作为参数，返回是否流中的元素都满足给定的条件，或至少有一个满足条件;以及是否没有满足条件的元素
- `collect(Collector<? super T, A, R>)`用于收集流参数到给定的集合中;这里的`Collector`是一个可修改的归并操作运算抽象；
 常用的方式是使用`Collectors`这一工具类提供的静态方法传入各种各样的`Collector`
- `collect(Supplier<R> supplier, BiConsumer<R, ? extends T> accumulator, BiConsumer<R, R> combiner)`是上述接口的一个简化版本，显示提供了归并操作的其中三个函数参数
- `count`返回元素的个数
- `toArray`返回元素为数组形式
- `forEach`则提供遍历操作
- `findFirst`/`findAny`返回第一个/任意一个元素，由于可能不存在希望取的元素（没有元素的情况），返回类型是`Optional<T>`
- `reduce`归并操作，包含几个重载形式

### 归并
归并操作是map/reduce模式中比较复杂和灵活的终止操作；`Stream`接口也提供了丰富的支持。其实这里的`reduce`在其它的函数式语言中也被称为`fold`。

#### 最灵活的形式

一般形式的`reduce`操作支持如下几个参数
- `U identity`是单位元元素，同时作为输出的默认值
- `BiFunction<U, ? super T, U> accumulator` 负责中间的每一次运算的累加
- `BinaryOperator<U> combiner` 约束accumulator 和 identity的函数，需要满足 `combiner.apply(u, accumulator.apply(identity, t)) = accumulator.apply(u, t)`

调用的效果类似于

```java
U result = identity;
for (T element: this stream)
  result = combiner.apply(result, accumulator.apply(identity, t))
return result
```

#### 省略`combiner`的版本: `T reduce(T identity, BinaryOperator<T> accumulator)`
该形式下，accumulator的类型是`BinaryOperator<T>`,同时`identity`的类型必须和Stream中的元素类型相同。

`sum`/`max`/`min`等都可视作是该版本的特化实现；即
```java
Integer sum = integers.reduce(0, (a, b) -> a + b);
//same as
Integer sum = integers.reduce(0, Integer::sum);
```

#### 只有`accumulator`的版本
这种情况下，返回的类型是`Optional<T>`;由于没有默认值，因此返回合适的结果必须至少有2个元素可以参与累加器运算；如果少于2个则返回空的Optional。
等价的代码为

```java
boolean found = false;
T result = null;
for (T element: this stream) {
  if (!found) {
    found = true;
    result = element;
  } else {
    result = accumulator.apply(result, element);
  }
}
return Optional.ofNullable(result);
```
该种形式的操作将Stream和Optional结合了起来。

## 总结

在函数式编程变得日益火热、几乎无处不在的今天，传统的基于面向对象范式的Java语言最终也通过**更新语言核心**的形式**拥抱**这一新的编程范式。
其设计从根本上来说，依然是基于围绕着传统的面向对象的抽象、封装、接口形式实现的，处处体现着面向对象编程的影子。
它对`Optional`类型的丰富支持和多样的接口抽象能极大地方便程序员的日常使用；强大的`Streams API`提供了基于流水线的高层抽象；
并发流的支持使得仅仅需要加入一个简单的`parallel()`调用即可实现无状态运算的并行化。

同时，一些"历史包袱"也深深地影响着Java的实现方式，包括基本类型不是对象，以及泛型接口的类型设计特性等导致某些接口看起来比较臃肿和罗嗦。
早期Java的简单性早已经消失殆尽了，因为它同样随着时间的推移加入了面向对象、泛型编程、注解、函数式编程等复杂的特性；
也许这是作为大规模采纳的程序语言不可避免的老路。

总体上来看，这些问题依然是瑕不掩瑜，大部分情况下普通程序员不需要被困扰过多。
随着时间的推移，业界的大规模采纳应该不是什么问题。
