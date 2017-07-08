---
layout: post
title: Excessive null checks in Java Defensive Programming?
categories: [programming, language, design]
tags: [programming, java, design, patterns, fp, language]
---

最近在参与某开发小组的[团体代码审查](https://atendesigngroup.com/blog/group-code-reviews)会的时候，发现组内的一线开发工程师对于何时应该做空指针检查并没有很清晰的认识；然而这在Java社区里早已经是个老生常谈的争论 。

<!--more--> 

按照最偷懒的做法 (毕竟[懒惰是伟大程序员的美德](http://threevirtues.com/)之一)，可以对使用的对象不做检查，那么万一该指针为空，则运行期抛出的空指针异常**默认行为就会将程序给crash掉**，你的用户会很不高兴，你公司的客户会不高兴乃至投诉你的研发老板，你的老板会更加不高兴甚至于愤怒以至于半夜将你叫起来加班解决问题；试想当你拖着睡眼惺忪的躯壳爬到办公室查看日志，迷迷糊糊一看，原来是有个地方的对象引用是null指针，没有做判断导致JVM退出了。那么最自然的方案是添加个判断了，原来的代码可能是 

```java
someObj.doSth()
```
现在被改成了
```java
if (someObj != null) {
    someObj.doSth()
}
```
后边可能出现了新的问题，那么你可能依法炮制，继续加上更多的分支判断。对于简单的代码片段而言这样做没什么问题，足够清晰易懂；只是实际的业务逻辑代码可能布满了各种各样的业务判断，再掺入这类判断代码，**可维护性立刻直线下降**。这时候可能有个聪明的程序员跳出来说，我们不是有[防御性编程](https://en.wikipedia.org/wiki/Defensive_programming)大法吗，可以来的更彻底一些，**干脆对所有的传入对象做空指针检查**不就可以了吗？

乍看起来似乎很有道理，转而仔细想一下就会发现这里**有很大的隐患**。上述的检查其实对每一次对象的调用其[复杂度](https://en.wikipedia.org/wiki/Cyclomatic_complexity)就会增加1，实际的业务代码里，一个类往往**引用了很多对象**，每次都要去做判断很快使代码变得难以维护，尤其是考虑到一些稍微复杂一些的方法可能有多个出口的情况，条件判断的增多会直接导致内部状态变得难以分析。本质上对某个方法的实现上下文来说，当前**所有的成员对象和传入的上下文参数对象**是否为null的状态都需要按照这个策略去来判断，在没有明确的前置条件约束的情况下，具体实现的业务逻辑会被埋藏在各种繁杂的空指针检查中难以理清。

显然对所有的对象引用做空指针检查不是一个明智的主意。那么是否有什么方法可以减少这些检查或者有其它的提高可读性的变通之道吗？

## 哪些情况需要判断空指针

为了探讨这个问题，我们可以详细列举一些可能需要判断的情况逐一分析一番就能做到心中有数了。因为Java语言中一切皆是对象(当然基本类型不算)，所有对象都由某个class给构造出来，所以我们**仅仅需要讨论class中涉及到的对象**即可。

### 成员对象 (field)
一个Java类的基本元素是其成员引用 (其实是对象指针) ，因为从成员对象开始探讨是个不错的主意。Java语言本身允许两种类型的成员对象：

#### 在构造中被初始化过的成员对象
构造中初始化的对象，其是否为null指针其实跟该类的设计密切相关，因为构造方法是对象"出生"的地方；其实现直接决定了构造出来的对象的内部状态。这种情况下，对象是否为null是个**比较关键的设计决策**之一。

对于这些纯粹是被使用到的对象(即其生存周期不由本class决定)，**只接受合法的构造参数并确保总是生成带有非null引用**的成员对象，会大大减少类内部需要检查空指针的情况。这样类的职责也很容易做到单一而明确；对应的其它方法里边就无需判断某个成员对象是否为null，当然如果其他的方法中会修改某个成员为null，那么情况会变得复杂 - 当然这种情况下出现的时候，往往意味着某些方法之间在使用成员对象来存储中间状态，很可能这些方法之间的隐式依赖需要仔细分析重构。

如果构造方法中的传入**参数非法而导致某些成员对象没法被正常构造**出来，那么我们的确可以采用防御式编程的思路，在构造里抛出来异常即可。当然这里有些Java语言特殊的情况需要仔细考虑，譬如是否需要抛出**待检查异常**还是非检查异常；并且在抛出异常的时候需要仔细小心留意没有资源泄露的情况，因为Java里没有C++中的RAII的概念，如果抛异常前已经分配的资源没有合适的释放(必须是手动的), 那么**调用者永远也没有办法处理**！

尽管有以上诸多细微之处需要留意，在构造方法中考虑好对象的初始状态约束，是减少不必要的null指针检查的第一步(可能也是非常关键的一步)。

#### 没有在构造中初始化的成员对象
没有在构造中初始化的成员对象，可以有几种不同的情况。然而毕竟**不能确保初始化对象的方法在什么时候被调用**了(除非有合适的设计约束)，因此成员方法中用到该对象的地方，都需要做空指针检查，否则就有可能出现意外的异常。当然详细的分析一下具体的场景也有助于我们做更深入的分析。

一种常见的典型场景是通过`setter`方法来初始化的对象，从纯粹面向对象设计的角度来考虑，简单的`setter`方法其实是**破坏封装**的，应该尽量避免。然而一种合理的情况是希望解决类之间的环形初始化依赖的情况，譬如依赖注入框架中的`setter injection`；当然环形依赖的情况之所以出现，往往意味着没有仔细的遵循[基于接口编程](https://stackoverflow.com/questions/383947/what-does-it-mean-to-program-to-an-interface)的原则。即使需要用，也应该尽量避免滥用。

还有一种常见的情形是在两个相互关联的方法之间传递中间状态；这种情形还可以再仔细甄别 - 如果是在两个共有方法之间共享状态，那么需要仔细考虑为何不在构造方法中设置好初始状态，也许[空对象模式](https://en.wikipedia.org/wiki/Null_Object_pattern#Java)是个值得考虑的好主意。如果是在两个私有方法或者是一个公有方法和其内嵌的私有方法之间传递状态，那么往往意味着代码的实现出现了**坏味道**，因为方法实现之间传递隐式状态很可能意味着本**类的职责过多需要拆分**出来新的类，或者是用空指针来传递控制流程迁移，这类不必要的空指针检查完全可以避免；因为私有方法可以看作是实现细节的隐藏，在实现内部具体的状态迁移都应该是严格受控的，要么通过合理的参数来传递，要么考虑好具体场景而不宜随意传递可能为空的对象。

### 静态成员对象 (Field)

静态对象是个和全局变量类似的问题，其初始化的时机其实已经超出了具体类对象的范围，会和其他的类产生**明显的强耦合**。因而静态成员对象应该尽量去避免，如果真要使用，确保其生存期被正确的管理，譬如使用依赖注入框架等。

### 方法传入参数对象
方法传入的参数对于方法的实现体而言是外部输入，因此也值得仔细判断。

#### 公有方法中的参数对象
显然共有方法中引入的参数对象是由调用者指定的，方法实现中需要考虑好什么样的参数是允许的，是否允许空指针传入；如果不期望空指针传入，那么可以在**类设计中考虑进去并写入JavaDoc**，用具体上下文信息封装一个合适的异常传递给调用者，并保证方法调用前后，类的状态仍然是合理而符合预期的。如果允许方法中的某些参数对象为null，并依据是否为null执行不同的行为，那么建议应该尽量避免这样的设计, 空指针控制业务流程和常规的直觉不符,完全可以通过其他方式传入，或者增加新的公有方法或重载版本。仔细考虑之后，仍然决定允许为空指针，**同样要在JavaDoc中写好**，方便你的用户。

#### 私有方法中的参数
私有方法往往是为想隐藏公有方法中的实现细节而准备的。显然由私有方法中引入的null类型参数是在实现某个公有方法的封装中引入的；由于公有方法的参数中没有带入这类空指针（见上述讨论），那么出现的空指针参数必然是由实现逻辑的过程中传入的；这**往往意味着实现中出现了不自然的设计**，这种情形和上述的共有方法传入空指针还不一样，因为私有方法的用户始终是这个类的某个公有方法（或者是其基类)，那么更没有理由不去重构了。

##### 一个具体的例子
譬如某个类有如下的实现方法

```java
class SomeClass {
  //....
  public void doSomeThing(SomeParam param) {
    //do something but not check param!
    if (someConditionMet()) {
      doSomeLowlevelWork(param, null);
    } else {
      //construct another param returns a new valid param
      doSomeLowLevelWork(param, constuctAnotherParam());
    }
    //....
  }

  private void doSomeLowlevelWOrk(SomeParam param, AnotherParam anotherParm) {
    //!oops, have to check param & anotherParam
    if (param == null) {
      //dosomething 
      return;
    }

    if (anotherParam == null) {
      //do something special
    } else {
      // do something as normal
    }
  }
}
```
这里的公有方法没有检查其传入的参数就直接将其传入了一个更底层的实现，并且显示的传入了另外一个null指针作为某个特殊的条件逻辑;这里的空指针用于决定底层实现逻辑的分支选择其实引入了不必要的跨逻辑层次依赖，属于比较明显的代码坏味道，可以将其重写如下 

```java
  //...
  public void doSomething(SomeParam param) {
    if (param == null) {
      //do something special
      return
    }

    doSomeLowlevelWork(someConditionMet(), param);
  }

  private void doSomeLowlevelWork(boolean conditionMet, SomeParam param) {
    //Now I'm sure no null passed in!
    if (conditionMet) {
      //do some thing without another param
    } else {
      AnotherParam anotherParam = constructAnotherParam();
      //do something else with another param, no null objects!
  }
```
重构后的版本将外部传入参数的判断放在公有方法的顶层处理，来确保相对底层的私有实现中不引入额外的空指针，取而代之的是具体的业务逻辑条件判断；代码的可读性显然有了良性的变化。

##### 例外情况

凡事总有例外，这里毫无疑问也存在一种特殊的情况，即是在实现[模板方法模式](https://sourcemaking.com/design_patterns/template_method)的时候，在基类中将空参数给传递进来了；这种情况下**如果不能修改基类的代码，那么依然不得不做处理**。

譬如在一个抽象类中定义了模板方法操作 

```java
public abstract class AbstractBehavior {
  
  //both firstParam and secondParam can be null, but not checked in below public API
  public void doSth(Param firstParam, AnotherParam secondParam) {
    doStep1(firstParam);
    //certain other handling
    doStep2(secondParam);
  }

  abstract void doStep1(Param firstParam);
  abstract void doStep2(AnotherParam secondParam);
}
```

在一个具体的实现类中，因为抽象类未做检查，子类实现必须检查父类传入的参数

```java
public class ConcreteBehavior {
  @Override
  void doStep1(Param firstParm) {
    //have to check if firstParam is null or not!
  }

  @Override
  void doStep2(AnotherParam secondParam) {
    //have to check if secondParam is null or not!
  }
}
```

话说回来，继承一个无法修改代码的基类可能往往不是一个很好的注意，这种情况下，[使用组合而不是继承](https://en.wikipedia.org/wiki/Composition_over_inheritance)可能是一种更好的方法，如果可以的话；因为从代码的耦合上来说，**继承关系是一种很强的耦合**以至于所有父类中的不良设计会被所有的子类所继承，形成无形的约束。这也许是我们需要始终对以高复用为目标的框架保持谨慎的原因。

### 外部对象

所谓的外部对象是这些在某个方法实现中被引用到，却并没有被类如方法传入参数，也没有被放在成员对象列表中的对象。当然如果出现这种情况，往往是全局对象的引用（要么是静态全局对象，要么是单例对象）。然而不管哪种情况下，你的class已经悄无声息地引入了**隐式依赖**，而隐式依赖在大部分情况下引入的问题比解决的问题要多。

对于这类对象，如果用现代的依赖注入方案来解决，很自然它们就和普通的成员对象没什么区别了。关键的问题是需要考虑好，是否真的必须引入这类隐式依赖就可以了，绝大部分情况下，**显示依赖比隐式依赖要好**。当然去掉了隐式依赖后，一个额外的好处是你的测试将变得更加容易了，因为不需要特殊的mock或是Stub来设置上下文了；只需要构造被测试对象的时候安插好构造参数即可。

## 变通之道
啰嗦这么多，看起来很多地方可能还是不可避免需要去做空指针检查。是否有办法做个变通，既保证逻辑正确，也能确保代码维护性不被破坏？其实回头仔细想一下，之所以有空指针异常这回事儿，根本上还不是因为**Java用异常机制来非正常情况的**处理吗，从这个角度出发，其实我们还有这些选择 

1. 用返回值而不用异常 - 然而我们是在讨论Java，虽然依然可以用返回值对象，或者类似C或者golang的error code的方式，但是如果你以这种方式写代码，其实**可维护性的负担反而加重**了,顶多是少了一些花括号而已
2. 使用**新的编程范式**，没错我们还有[函数式编程]({{site.baseurl}}/categories/fp/index.html)可以选择，因为Java8已经给我们送来了这个大礼 - Optional

### Optional
Java8新引入的[Optional](https://www.mkyong.com/java8/java-8-optional-in-depth/)类型提供了不同于传统基于返回值或者基于异常的新的错误处理机制。一个Optional类型是一个包装类型，其封装了原有的对象类型，但是在对象的状态上，允许表达该对象**要么是存在要么是不存在**的概念。表面上看起来和传统的null没有太明显的区别，然而两者之间有很大的不同

譬如同样一个可能为空的对象上的一个多步骤操作，传统的方式可以写为 

```java
ResultType result = someObj.doSth();
if (result != null) {
  return doSomethingWith(result);
} else {
  //exceptional handling code, other return
  return doExceptionalHandling();
}
```

如果换用Optional封装则可以写为

```java
return Optional.ofNullable(someObj.doSth())
    .map(result -> doSomethingWith(result))
    .orElseGet(() -> doExceptionalHandling());
```

- 传统的空指针或者返回值方式返回的对象是不同的类型，程序员必须**对返回值做类型相关的处理**，从这种意义上说，返回值方式(包括空指针)提供的相对低层次的封装，毕竟对于C++/Java这类强类型语言而言，不同的变量类型相关处理意味着底层语言基础设施和应用业务逻辑这两个不同层次的抽象被混杂在同一个层级的代码范围内
- `Optional`类型提供的是统一的对象类型封装，你可以对该类型做相对更高层次的封装，根据你具体的业务逻辑写出声明式的代码

### 隐藏在Optional中的模式
从上面的示例代码可以看出，Optional类其实提供了关于一些基本过程逻辑的封装，使得使用者可以**站在更高的层次写代码**，关于一些基本的分支判断等过程逻辑控制，Optional提供了一些模式给程序员来调用，使得程序员可以更加关注在业务逻辑上，减少程序语言的底层实现细节的纠缠，同时又**不丢失静态语言带来的编译器检查**的便利。

当然Optional类型本身是一个封装类，作为函数式编程中的一个模式在Haskell中它是[一种具体的`Monad`抽象]({{ site.baseurl }}{% link _posts/2012-04-15-haskell-functor-and-monad.markdown %})，其提供的方法提供了各种各样关于封装对象的高层操作，包括过滤器/映射/异常处理等，可以实现更复杂而高级的操作，这里不再赘述；细节可参考Optional的使用。

### Java8的 Type Annotation
其实在Java8中，新引入的类型注解针对空指针问题提供了另外的处理方式，即通过指定`@Nonnull`，编译器可以用于检测某个代码路径中有可能接受到空指针的情况，从而避免程序员处理空指针异常。IntelliJ IDEA提供了贴心的提示建议插入这个annotation从而帮助我们写出更整洁的代码。

## 参考引用
1. [Defensive programming, the good, the bad, the ugly](http://enterprisecraftsmanship.com/2016/04/27/defensive-programming-the-good-the-bad-and-the-ugly/)
1. [Is it a good practice to make constructor throw exception](https://stackoverflow.com/questions/6086334/is-it-good-practice-to-make-the-constructor-throw-an-exception)
1. [Composition over inheritance](https://en.wikipedia.org/wiki/Composition_over_inheritance)
1. [Java8 new type annotations](https://blogs.oracle.com/java-platform-group/java-8s-new-type-annotations)
