// web_page.cpp —— 页面骨架（原 template/index.html），现在就是一段 C++ 字符串。
//
// 只有 <head> 里那一小段脚本是内联的：它必须在首次绘制之前给 <html> 挂上
// gate-on / motion-auto / has-wallpaper，否则会先看到正文再被门盖住，闪一下。
// 其余脚本都在 fx.js / site.js 里，同样由 C++ 生成。

#include "assets.hpp"

namespace ssg {

std::string webPageHtml() {
    return std::string(R"HTML(<!DOCTYPE html>
<html lang="zh-CN" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{{name}} · {{title}}</title>
<meta name="description" content="{{tagline}}">
<meta property="og:title" content="{{name}} · {{title}}">
<meta property="og:description" content="{{tagline}}">
<meta property="og:type" content="profile">
<meta property="og:image" content="{{og_image}}">
<meta name="theme-color" content="{{accent}}">
<link rel="icon" href="favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="style.css">
<script>
/* 首次绘制之前定下几件事，避免闪烁：
   1) 要不要显示进场门   2) 动效是否跟随系统   3) WELCOME 动画是否强制播   4) 有没有壁纸 */
(function () {
  var r = document.documentElement;
  if ({{gate_enabled}}) r.classList.add('gate-on');
  if (!{{motion_always}}) r.classList.add('motion-auto');
  if ({{welcome_always}}) r.classList.add('welcome-always');
  if ('{{wallpaper}}') r.classList.add('has-wallpaper');
})();
</script>
</head>
<body>

<div id="progress" aria-hidden="true"><i></i></div>

<!-- 背景层：像素火焰。贴在内容下面，透过磨砂面板透出光 -->
<div id="fx-fire" class="fx-layer fx-back" aria-hidden="true"><canvas id="fire-canvas"></canvas></div>
<!-- 前景层：跟随鼠标的像素粒子 -->
<div id="fx-cursor" class="fx-layer fx-front" aria-hidden="true"><canvas id="cursor-canvas"></canvas></div>

{{gate}}
{{welcome_layer}}

<button id="nav-toggle" type="button" aria-label="打开目录" aria-expanded="false">
  <i></i><i></i><i></i>
</button>
<div id="nav-scrim" class="nav-scrim" hidden></div>

<aside id="sidebar" class="sidebar" aria-label="站点目录">
  <i class="side-rail" aria-hidden="true"><b id="side-progress"></b></i>

  <a class="side-brand" href="#top">
    <img class="side-avatar" src="{{avatar}}" alt="{{name}}" width="46" height="46">
    <span class="side-brand-txt"><b>{{name}}</b><i>{{title}}</i></span>
  </a>

  <div class="side-search">
    <input id="side-q" type="text" autocomplete="off" spellcheck="false"
           placeholder="快速查找…" aria-label="快速查找内容">
    <span class="side-key" aria-hidden="true">/</span>
  </div>
  <div id="side-hits" class="side-hits" hidden></div>

  <nav id="side-nav" class="side-nav" aria-label="内容导航">
    <i id="side-pill" class="side-pill" aria-hidden="true"></i>
{{nav}}
  </nav>

  <div class="side-foot">
    <button id="theme-btn" type="button" aria-label="切换深浅色">
      <span id="theme-icon" aria-hidden="true">◐</span><em>主题</em>
    </button>
    <a href="{{github}}" target="_blank" rel="noopener">GitHub</a>
    <a href="mailto:{{email}}">邮箱</a>
  </div>
</aside>

<div class="shell">
<main id="top">

  <section class="hero">
    <div class="hero-avatar">
      <img class="avatar" src="{{avatar}}" alt="{{name}}" width="132" height="132">
    </div>
    <div class="hero-text">
      <p class="eyebrow">{{location}} · ONLINE</p>
      <h1>{{name}}</h1>
      <p class="role">{{title}}</p>
      <p class="tagline">{{tagline}}</p>
      <div class="cta">
        <a class="btn btn-primary" href="#projects">看看我做过的</a>
        <a class="btn" href="#core">核心面板</a>
        <a class="btn" href="mailto:{{email}}">联系我</a>
      </div>
      <ul class="hero-stats">{{stats}}</ul>
    </div>
    <i class="hero-scan" aria-hidden="true"></i>
  </section>

  <section id="about" class="block reveal">
    <h2><span class="num">01</span>关于我</h2>
    <div class="prose">{{about}}</div>
  </section>

  <section id="skills" class="block reveal">
    <h2><span class="num">02</span>技能</h2>
    <div class="skills">{{skills}}</div>
  </section>

  <section id="projects" class="block reveal">
    <h2><span class="num">03</span>项目</h2>
    <div class="cards">{{projects}}</div>
  </section>

  <section id="timeline" class="block reveal">
    <h2><span class="num">04</span>经历</h2>
    {{timeline}}
  </section>

  <section id="core" class="block reveal">
    <h2><span class="num">05</span>核心面板</h2>
    <p class="block-lead">一块虚拟芯片：四周的电流往中间涌，中间印着这台"站点机器"的规格。</p>
    <div class="board" id="core-board" data-label="{{chip_label}}" data-sub="{{chip_sub}}">
      <canvas id="chip-canvas" aria-hidden="true"></canvas>
      <div class="board-hud">{{core_specs}}</div>
      <i class="board-corner tl" aria-hidden="true"></i>
      <i class="board-corner tr" aria-hidden="true"></i>
      <i class="board-corner bl" aria-hidden="true"></i>
      <i class="board-corner br" aria-hidden="true"></i>
    </div>
  </section>

</main>

<footer>
  <div class="footer-inner">
    <p class="footer-cta">有想法？<a href="mailto:{{email}}">给我写封邮件</a></p>
    <p class="footer-note">{{footer}}</p>
    <p class="footer-note">© {{year}} {{name}} · 源码是 C++，页面由 C++ 生成</p>
  </div>
</footer>
</div>

<script type="application/json" id="site-index">{{index}}</script>
<script src="fx.js"></script>
<script src="site.js"></script>

</body>
</html>
)HTML");
}

}  // namespace ssg
