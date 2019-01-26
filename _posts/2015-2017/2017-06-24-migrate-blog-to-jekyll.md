---
layout: post
title: Migrate blog to Jekyll
categories: [ blog ]
tags: [ blog ]
---
之前本blog的内容是基于强大的[octopress](http://octopress.org/)生成为静态站点，然后将生成的内容静态拷贝到 github-pages 上去的。当octopress的作者宣布重构后的[新版本预告](http://octopress.org/2015/01/15/octopress-3.0-is-coming/)后便急冲冲上去升级了本站内容，可惜很多插件都不工作了。眼见2年多过去了，貌似正式的3.0版本还没有被宣布，官方页面的帮助依然停留在2.0版本时代。

很明显，**octopress项目要死了**，这在开源软件社区是常有的事儿；但是活人不能被尿憋死，本来用octopress的目的也是因为 Jekyll 太抽象了想顺带找个近路学习学习前端知识；这些没有了借口，还是升吧。

<!--more-->

## 上新版本Jekyll，安装主题

最开始用octopress的时候，Jekyll还是 0.6 版本；这么多年过去，Github 团队官方已经发布了 3.0 版本了，功能支持的已经比较完善了。本身Jekyll是基于 gem 的，模块化做的非常好了，不像原来的 octorpess 直接fork一个代码就在里边改来改去，还要创建多个branch 防止无法升级的问题。按照 [官方文档](https://jekyllrb.com/docs/installation/) 的步骤按部就班即可。
 
Jekyll支持**丰富的插件**，生成好对应的site之后，修改配置文件就可以用 `bundle update` 来安装。但是默认的Jekyll主题太过于原始，所以找个适合自己口味的主题是首要的。因为本blog是托管在github上的，gith-hub pages [支持的主题](https://pages.github.com/themes/) 却**比较有限**, 而且要没没有提供预览，要么提供的预览不适合个人口味。

回头看Ocotpress发现其实现方式完全是自己生成页面的，不用Github自己的build系统；这里可以很容易的依法炮制，自己将所有的东西编译好，将生成的site内容给传上去即可。个人还是比较喜欢原来的 Octorpress提供的默认主题，简洁明快；所以最终选择的是 [Minimal Mistakes](https://mademistakes.com/work/minimal-mistakes-jekyll-theme/) ；浏览选择过程中，对 [Feeling Responsive](http://phlow.github.io/feeling-responsive/)主题也非常入眼，只是看起来文档比较复杂，可以留待以后尝试折腾下。

Minimal Mistake主题支持比较多的设定，因为打算自己生成页面，所以采用类似fork的思路，安装**自己想要的插件**（可以突破[Github-Pages的限制](https://pages.github.com/versions/))，最后build成页面给push上去即可。

## 自动同步

由于是自己手工编译生成静态内容，所以很希望有类似于Octopress的Rakefile，遗憾的是**copy下来的Rakefile**不能正确执行。Google上search了一圈发现没有现成的方案，而且Github-pages的个人站点仅仅**支持push到master分支**, 而~~早期的时候是push到gh-pages~~分支的；这个细微的更新貌似很多google页面都没有提及；不过好在也不是很难发现，仅仅需要在自己site的设置页面查看build的分支就可以看到只能接受master分支了。

自动化的工作最终写了个简单的shell脚本来完成, 基本的步骤是沿用 Octopress2 的步骤，设置2个入口
1. 一个用于初始化一个deploy目录，和github的站点内容保持同步，初始化git和remote等等。
2. 另外一个用于每次更新内容，调用 `jekyll build` 完成编译后，将生成的内容全部copy到deploy目录下，然后再同步到github的repository

```bash
#!/bin/bash
deployDir=$(pwd)/_deploy
siteDir=$(pwd)/_site
siteUrl=https://github.com/skyscribe/skyscribe.github.io.git

function setup() {
    rm -fr $deployDir
    mkdir -p $deployDir
    cd "$deployDir"
    git init
    echo "dummy content" > index.html
    git add .
    git commit -m "dummy script init"
    git branch -m master
    git remote add origin "$siteUrl"
    echo "Setup complete for $deployDir"
}

function deploy() {
    bundle exec jekyll build
    cd "$deployDir"
    cp -r "$siteDir"/* "$deployDir/"
    git pull origin master
    git add -A
    now=$(date "+%Y-%m-%d_%H:%M:%S")
    git commit -m "site updated at $now"
    git push origin master --force
    echo "deploy completed"
}

case $1 in
  setup)
	    setup
        ;;

    * )
        deploy
        ;;
esac
```

## 数据搬运

相对来说数据搬运不是很困难就是比较繁琐，原来很多Octopress支持的插件，在这里都得自己检验是否不工作或者是否有合适的插件可以替代。期间的过程就是不断的**测试驱动**搬运，发现编译出错后，打开`--trace`开关，看问题出在什么地方然后一一修改之。最麻烦的一个问题是 gist 插件貌似产生了不兼容，原来的 `include_file` 插件也没法再用，只好将文件内容copy过来，用Liquid的 `highlight` 和 `include` 标签，套用如下的模板 

{% raw %}
    {% highlight html %}
        {% include code/include.html %}
    {% endhighlight %}
{% endraw %}

## 插件和定制

没有了github的插件束缚，就可以装自己想要的插件了
1. jekyll-compose 可以用**命令行来生成新的post**，类似于之前的octopress的命令行
2. jekyll-archives 可以用来生成归档页面，方便使用

默认的blog页面用的是post layout，但是默认没有定义，简单起见，自己在 `_layout` 下边加一个就行，从默认的single.html拷贝过来修改一下即可。

### Tag cloud
Octopress的tagcloud插件，找了一圈也没找到好的，仿照别人家的，写了个简单的 tagcloud.html 

{% raw %}
    {% assign all_tags = site.tags | size %}
    <h3>
      <a href="{{absolute_url}}/categories/index.html">Categories</a>
    </h3>
    <div class="tagCloud">
      <ul>
      {%for tag in site.categories %}
      {% assign category_name = tag | first %}
      {% assign cat_count = tag | last | size %}
      {% assign cat_avg = cat_count | div: all_tags %}
      <li>
      <span class="tag">
          <a href="{{absolute_url}}/categories/{{ category_name | downcase}}/index.html">{{ category_name }}({{ cat_count }})</a>
      </span>
      </li>
      {% endfor %}
      </ul>
    </div>
{% endraw %}

稍微定制下需要显示的地方，将上述html给include进去即可。

### 布局调整
页面在高分辨率的屏幕下感觉空白太多，进入 `_sass/minimal-mistakes` 修改即可，代码相对比较清楚
- `_variables.scss` 包含一些全局变量的定义，譬如字体大小，缩进，padding等
- `_sidebar.scss` 包含对边栏的样式定义
- `_masthead.scss` 指定主头部元素的样式定义
- `_page.scss` 包含页面主要内容部分的样式定义

大概对着浏览器窗口调整即可，无需赘述。

## 未尽事宜
还有一些其它的问题暂时没找到解决方案，暂时不折腾了，先列在这里。

#### 无法删除的老页面
之前的blog采用的是octopress的URL设置，当在新的主题里，将permalink依法炮制的时候，发现更新的页面没有被github正确重新加载，依然显示的是老的页面。无奈之下，只好修改一下permalink的前缀部分，强制生成不一样的页面。问题是老的页面依然可以访问。

#### 页面布局
默认的布局中有很多*额外的padding*没法去掉，尤其是PC的分辨率比较高的时候，页面右侧的边框显得特别大；用Chrome的Inspect工具打开可以看到右侧的padding有16%之多，然而修改附带的css并不能生效。

## 参考链接 
1. [How I migrated my blog to Jekyll/Github](http://rafabene.com/2015/10/11/how-migrated-blog-jekyll-github/)
2. [Include partial snippets of code](https://hblok.net/blog/posts/2016/10/23/jekyll-include-partial-snippets-of-code/)
3. [Raw tag plugin to prevent liquid from parsing given text](https://gist.github.com/phaer/1020852)

