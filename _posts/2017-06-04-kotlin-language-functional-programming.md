---
layout: post
title: Kotlin语言之函数式编程
date: 2017-06-04 22:32
comments: true
categories: [language, programming, fp]
tags: [kotlin, programming, language, fp]
---

[Kotlin语言](https://kotlinlang.org/)是大名鼎鼎的JetBrains公司（就是可以甩Eclipse数条大街的IntelliJ IDEA背后的公司）出品的现代的编程语言，之前已经在IDEA中蹦达出来很多次了；只是最近随着Google在其[2017年的I/O大会上将其列为Android平台官方支持的语言](https://techcrunch.com/2017/05/17/google-makes-kotlin-a-first-class-language-for-writing-android-apps/)而窜上了热点。

本文尝试从函数式编程的角度管窥Kotlin的特性。

## JVM上的函数式语言生态
作为一门比较年轻的编程语言，要想在既有的数百种语言中脱颖而出，成功吸引开发者的心，对新的[函数式编程范式](https://en.wikipedia.org/wiki/Functional_programming)的支持是必然不可少的 - 这一点基本成为语言出品商心照不宣的潜规则了，当然在21实际，不支持面向对象的范式也是说不过去的。

作为基于JVM平台的语言，和Java的互操作性肯定是一个重要的优势，当然这方面已经有成熟的函数式语言[scala](https://www.scala-lang.org/)和更早一点的[clojure](https://clojure.org/about/rationale)在前。可能比较遗憾的是，正统的函数式编程风格太难被传统的OO程序员所接受，因此基于传统Lisp的clojure一直曲高和寡，scala在近年来有变得更加流行的趋势，只是目前看来[仍然没有跨越期望的引爆点](https://dzone.com/articles/the-rise-and-fall-of-scala)。

### 有丰富的特性还希望有速度
传统印象中的静态函数式语言的编译速度往往会比较慢，这一点在工程实践上是个很重要的因素。

Kotlin作为后来者，其开发者认为静态语言的编译速度是个至关重要的，然后Scala的编译速度远不能令人满意。对大型的项目而言，笨拙的编译速度浪费的可是大量的时间和金钱；毕竟天下武功唯快不破，更快的编译时间意味着更快的反馈周期，更多次的迭代开发。Kotlin的目标之一是期望编译速度可以像Java一样快，[benchmark分析](https://medium.com/keepsafe-engineering/kotlin-vs-java-compilation-speed-e6c174b39b5d)也表明了二者的速度是差别不大的。

## 基本特性
函数式语言的基本元素就是function，这一点kotlin倒是没有玩太多花头。用`fun`关键字来声明函数，函数是第一等公民，可以支持函数作为参数，返回函数等基本特性。

### 不可变类型支持
Kotlin强制要求程序员声明某个特定的变量是否是可变类型。

如果是可变类型，则需要用`var `来声明；那么后续程序中任何地方访问变量都会被IDE给highlight出来，提醒可能的副作用。因为可变类型意味着内部存储着状态，从函数式编程的角度来看，状态会**影响函数的纯度**，带来副作用和复杂性。

![immutable_hints.png](http://upload-images.jianshu.io/upload_images/5275528-e33afde1447172d5.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

### 函数声明
基本的函数声明是这样的 
```kotlin
fun thisIsAFunction(x: Int) : Int {
}
```
当然这里的类型后置语法和传统的C家族语言有些不同，但是适应起来倒也不是难事儿。

### 类型推导
Kotlin也支持强大的类型推导，从而在很多情况下，可以省略不必言的类型指定，简化代码；譬如函数的返回类型可以被自动推断的时候，其类型声明可以被省略。

### 特殊的返回类型 `Unit`
`Unit`是一个特殊的类型，用于指定某个函数返回的值可以被省略，类似于Java8的`Void`类型。如果一个函数没有返回值，那么可以指定其返回`Unit`或者直接省略其返回

```kotlin
fun someFunc(arg: SomeType) : Unit {
    // do something with arg
    // no return needed
}

// same as above
fun someFunc(arg: SomeType) {
    // do something
}
```

### 中缀表达式
中缀表达式写法更替进人的思维习惯，在定义某些操作符的时候是非常有用的。此用法往往用于扩展已有类型的操作，定义的时候需要满足以下条件
- 属于某个类的成员函数，或者是定义某个类的扩展函数(后边再回头来看)，因为这里我们**必须知道左侧的操作对象**是谁
- 必须只有一个函数参数（操作符后边的对象）
- 用`infix`关键字来标记

譬如

```kotlin
infix fun Int.shl(x : Int) -> Int {
  /// implementation of shl operation
}

// call site
1 shl 2
```

### 命名参数和默认值
这点和Python很像在多个参数的复杂函数的使用上有很大帮助，能极大提高可读性减少维护成本。调用方可以在调用点指定需要传入的参数的名字；也可以省略掉不需要指定的参数。

譬如有如下的`reformat`函数用于格式化

```kotlin
reformat(str,
    normalizeCase = true,
    upperCaseFirstLetter = true,
    divideByCamelHumps = false,
    wordSeparator = '_'
)
```

调用点可以简单写作

```kotlin
reformat(str, wordSeparator = '_')
// equals to
reformat(str, true, true, false, '_')
```

这个功能在传统的C++/Java里边没有提供，但是IDEA提供了只能提示可以弥补Java的不足；而Kotlin则将其内置在语言中了；本身没多少复杂性在里边。

## 高阶函数和语法糖

### 高阶函数
函数的参数可以是一个函数，这个在Kotlin的库里已经有大量的例子，譬如基本的`Sequence`的filter函数携带一个谓词函数，其针对给定的参数返回一个` Boolean`
```kotlin
public fun <T> Sequence<T>.filter(predicate: (T) -> Boolean): Sequence<T> {
    return FilteringSequence(this, true, predicate)
}
```

### 单参数函数的表达式形式
当函数只有一行实现的时候，可以省略其函数体，直接用`=`来书写，就像复制给一个变量一样

```kotlin
fun add2Numbers(x : Int, y: Int): Int = x+y
```

### Lambda和匿名函数
匿名函数用大括号括起来，上面的例子也可以写作

```kotlin
val add2Numbers2 = {x : Int, y: Int -> x+y}
```

### 函数调用的形式省略
当函数仅仅有一个参数的时候，其参数名字默认为`it`保留关键字可以不用显示指定。

当函数的最后一个参数是一个函数的时候，其函数体可以用`{}`块的方式来书写，获得更好的可读性。

譬如如下的例子用于打印指定数目个偶数

```kotlin
val printEvens = { x: Long ->
    IntStream.range(1, 10000000)
            .filter { it%2 == 0 }.limit(x)
            .forEach { println(it) }
}
```

### 一个具体一点的例子

假设要实现如下功能的函数
1. 遍历某个目录树
2. 找出所有符合条件的文件夹
3. 取其文件绝对路径
4. 归并为一个字符串列表返回

可以通过如下几个函数完成
```kotlin
fun extractAllDomainDoc(dirName: String) {
    File(dirName).walkTopDown().filter { isDocDir(it) }
            .map { it.absolutePath }.toList()
}

private fun isDocDir(file: File): Boolean {
    return file.isDirectory && isDomainDocDir(file)
}

private fun isDomainDocDir(file: File): Boolean {
    return file.absolutePath.split(File.separator)[file.absolutePath.split(File.separator).size - 1] == "doc"
}
```
这里每个函数的含义都是比较清楚易懂的。如果利用上述的省略规则，那么可以更简略的写为

```kotlin
fun extractAllDomainDoc(dirName: String) = File(dirName).walkTopDown()
        .filter { isDocDir(it) }
        .map { it.absolutePath }.toList()

private fun isDocDir(file: File) = file.isDirectory && isDomainDocDir(file)

private fun isDomainDocDir(file: File) = file.absolutePath
        .split(File.separator)[file.absolutePath.split(File.separator).size - 1] == "doc"
```

## 类型扩展函数
Kotlin 支持对已有的类型添加扩展，值需要在任何想要的地方添加想要的功能，则原有的类型即可像被增强了一样具有新的功能，该机制提供了OO之外新的灵活的扩展方式。

譬如默认的Kotlin的`Iterable`类没有提供并发的`foreach`操作，可以通过扩展机制很容易的写出来一个使用`ExecutorService`来并发循环的版本

```kotlin
// parallel for each, see also https://stackoverflow.com/questions/34697828/parallel-operations-on-kotlin-collections
fun <T, R> Iterable<T>.parallelForEach(
        numThreads: Int = Runtime.getRuntime().availableProcessors(),
        exec: ExecutorService = Executors.newFixedThreadPool(numThreads),
        transform: (T) -> R): Unit {

    // default size is just an inlined version of kotlin.collections.collectionSizeOrDefault
    val defaultSize = if (this is Collection<*>) this.size else 10
    val destination = Collections.synchronizedList(ArrayList<R>(defaultSize))

    for (item in this) {
        exec.submit { destination.add(transform(item)) }
    }

    exec.shutdown()
    exec.awaitTermination(1, TimeUnit.DAYS)
}
```

这里在函数体中，`this`自动会绑定于被扩展的对象。

如果我们想实现一个自动将一大堆plantuml文件转换为png格式并copy到指定目录，因为默认的plantuml的API是单线程的，我们可以基于上述的parallelForEach实现来并发调度UML的生成过程，对应的代码可以写为

```kotlin
markDownFileLists.parallelForEach {
    SourceFileReader(File(it)).generatedImages.firstOrNull()?.apply {
        copyFileToDirWith(this.pngFile.absolutePath, getCopyTarget)
        println("${System.currentTimeMillis()} - Created png for $it")
    }
}
```
