---
layout: post
title: Kotlin语言之变量约束设计
date: 2017-06-05 23:12
comments: true
categories: [language, programming, design]
tags: [kotlin, programming, language,fp]
---
可变状态的泛滥往往被认为是软件维护灾难的元凶之一，尤其是当过程封装遇上多线程，普通的面向对象编程技巧完全不堪大用，因为**继承/封装/多态等手法针对的都是程序组织上**的处理措施，具体到底层实现上，传统的C/C++/JAVA依然依赖**过程式实现跟操作系统打交道**。

## 函数式编程里的副作用
在函数式编程的世界里，事情会变得很传统的过程式处理不一样，因为这里非常讲究函数本身是否是**有副作用**的，如果**同样的输入不能保证相同的输出**，那么则是有副作用的。这里的输出不仅仅表示返回值，还隐含其它形形色色的对环境的影响，包括

- 申请但是没有释放的内存
- 向操作系统请求占用共享资源如网络套接字
- 屏幕输出，磁盘占用等

## 为什么要区分副作用

显然，副作用引入了**额外需要程序员维护**的状态，而传统的线程库或基本的OS机制将其完全交给了程序员负责。从而导致在多线程编程环境下，复杂的问题随着状态的增加成**指数上升**。状态意味着有共享资源需要维护，当有并发执行的进程或是线程的时候，为了保证正确的程序语意，则不得不引入锁(昂贵的操作)和竞争，从而制约性能。无锁算法通过CAS+重试机制，可以**部分缓解锁的开销**，却不能从本质上解决问题。

无副作用的函数则是天然适合并发的，因为没有共享自然可以并行不悖地执行，问题不是完美解决了吗？然而**现实世界总是不允许绝对完美二字存在**的，纯粹无副作用的函数几乎一无是处，因为它本质上没什么用，什么也做不了。

退而求其次的想法是，能否尽量隔离两者的实现，然后又可以优雅地将二者集成起来完成实际功能？**HASKELL用其优雅的monad抽象**回答了这个问题。然而对于抽象思维能力不是那么强（或者没有那么好数学基础）的程序员而言，**Monad实在是太阳春白雪**了而难以接近；想更加接地气一点的程序语言无一不选择和Monad保持距离，即使某些构造和设计的思想就来源于Monad, 譬如随处可见的Optional，基本的map/reduce链式操作等。

对于这些没有显示引入monad的非纯函数式语言来说，严格的隔离就显得有些太激进了。取而代之的相对折中一点的**平庸**策略是语言机制本身提供某些基础机制，剩下的怎么用这些基本机制，一切由程序员自己来定夺。

## kotlin的语言层面基本机制

kotlin通过关键字 `val` 来声明**只读**的变量，用 `var` 来声明可变量。任何函数只要引入对可变量的使用，则其本身就是有明显的副作用的。然而一个变量声明为只读，仅仅表示在其对应的作用域中，不允许修改此变量的值，并**不意味着实际指向的数据对象本身是不可变**的， 因为在可能有其他的地方使用 `var` 的方式来操作此变量，或者有显示的方式将一个 `val` 的变量转换回可变的 `var`。

考虑下边的例子：
```kotlin
// field1 是只读的，在本class中不允许修改它
class SomeClass(val field1 : SomeType, var field2 : Int) {
   fun doSth() {
       // can only modify field2, but not field1
   }
}

//calling site
var someTypeInst = SomeType()
val obj = SomeClass(someTypeInst, 112)
// someTypeInst can still be changed by others! Not recommended!
obj.doSth() 
```
虽然`someTypeInst`是以只读方式传入`obj` 的，然而并不能保证没有其它的线程并发地修改实际的对象，如果发生这种情况，**程序员仍然需要保证数据的一致性和安全**。 

### 只读变量的初始化

显然不可变变量则仅仅能够初始化一次，后续使用中不能再修改了。这样也带来一些限制，譬如在 `init block` 里想一次性初始化某些资源然后将其设置为在class内部是只读，则无能为力。一种变通的方式是将其设置为 `var` 类，然而这样做我们就损失了只读约束；另外一种做法则需要使用property构造来封装。


## 核心集合类
kotlin对来自JAVA的集合类库进行了二次封装，清晰地划分了只读集合类和可变集合。

### 接口定义
常用的集合类接口在`kotlin,collections` 包中被重新定义 ( 源码中位于 `Collections.kt` )
```kotlin
package kotlin.collections 
//...
// by default not mutable
public interface Iterable<out T> {//... }

// mutable iterable supports removing elements during iterating
public interface MutableIterable<out T> : Iterable<T> {//...}

//Only read access to collection
public interface Collection<out E> : Iterable<E> {//...}

// Supports read/write operations
public interface MutableCollection<E> : Collection<E>, MutableIterable<E> {//...}
```

具体的集合类接口则选择从以上接口中**选择对应的**来扩展实现，因而对同一个类型有两种实现，分别是只读的 (没有前缀) 的和可变类型 (**用 Mutable 做前缀区分**) 。譬如 `List` 类就定义为 

```kotlin
// Read only list interface
public interface List<out E> : Collection<E> {//...}
// Mutable list
public interface MutableList<E> : List<E>, MutableCollection<E> {//...}
``` 

需要注意的是，实际的具体实现类是复用Java中的定义，可参考collection包中的 `TypeAliases.kt` 文件

```kotlin
package kotlin.collections
//...
@SinceKotlin("1.1") public typealias ArrayList<E> = java.util.ArrayList<E>
```
默认的集合操作以及Streams API返回的大部分是不可变接口对象。

### 集合类扩展/工具函数
除了使用默认的JDK实现来生成具体集合类对象，Kotlin标准库中同时提供了大量的封装函数方便程序员使用，某些来源于对JDK的直接封装，有一些则是直接inline实现。

譬如返回空list的包装和初始化形形色色的list
```kotlin
/** Returns an empty read-only list. */
public fun <T> emptyList(): List<T> = EmptyList

/** Returns a new read-only list of given elements.  */
public fun <T> listOf(vararg elements: T): List<T> = if (elements.size > 0) elements.asList() else emptyList()

/** Returns an empty read-only list. */
@kotlin.internal.InlineOnly
public inline fun <T> listOf(): List<T> = emptyList()

/**
 * Returns an immutable list containing only the specified object [element].
 * The returned list is serializable.
 */
@JvmVersion
public fun <T> listOf(element: T): List<T> = java.util.Collections.singletonList(element)
```

生成可变List的函数封装大多也是清晰明了 , 并且有很多种类的封装，使得就地生成 List 的工作大大简化；大部分情况仅仅需要**使用已有的函数**即可，不需要发明新的轮子

```kotlin
/** Returns an empty new [MutableList]. */
@SinceKotlin("1.1")
@kotlin.internal.InlineOnly
public inline fun <T> mutableListOf(): MutableList<T> = ArrayList()

/** Returns an empty new [ArrayList]. */
@SinceKotlin("1.1")
@kotlin.internal.InlineOnly
public inline fun <T> arrayListOf(): ArrayList<T> = ArrayList()

/** Returns a new [MutableList] with the given elements. */
public fun <T> mutableListOf(vararg elements: T): MutableList<T>
        = if (elements.size == 0) ArrayList() else ArrayList(ArrayAsCollection(elements, isVarargs = true))
```

其它集合类  (set/map等) 的实现原理**大概类似**，可以通过查看对应源码。

### 不可变集合转换为可变集合
很多场景下，API返回的都是不可变集合，将其变成一个可变对象再行编辑修改是常见不过的变成任务；kotlin 通过其**自身的扩展机制**将这些工具函数自动添加到了对应的集合类上

如果想要将一个只读的 `Array` 对象变为一个可变的 `MutableList`，那么其实现是通过重新初始化一个新对象实现的： 

```kotlin
// Below code is copied from generated standlib as _Arrays.kt
//  see https://github.com/JetBrains/kotlin/tree/master/libraries/stdlib

/**
 * Returns a [MutableList] filled with all elements of this array.
 */
public fun <T> Array<out T>.toMutableList(): MutableList<T> {
    return ArrayList(this.asCollection())
}
```

对于具体的Array类，有不同的实现，如 `ByteArray` 的初始化方法则有所不同，直接调用其构造函数，然后注意添加现有的各个元素
```kotlin
/**
 * Returns a [MutableList] filled with all elements of this array.
 */
public fun ByteArray.toMutableList(): MutableList<Byte> {
    val list = ArrayList<Byte>(size)
    for (item in this) list.add(item)
    return list
}
```
之所以如此，是因为具体这些子类是被映射到具体的 JVM 对象上的。如ByteArray的文档如是说 
> public final class ByteArray defined in kotlin
> An array of bytes. 
> When targeting the JVM, instances of this class are represented as `byte[]`.

而对于CharArray，则其映射到`char []`类型上去。

## IDEA支持
作为官方的IDE环境，IDEA对可变量的引用做了显示的**下划线**提醒，程序员可以一目了然地看到代码中对可变量的使用。

然而想要更深入的查看整个实现调用链中，哪些引入副作用哪些没有，工具的支持就比较有限了。

