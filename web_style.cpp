// web_style.cpp —— 样式表（原 template/style.css），以 C++ 字符串内嵌。
//
// 分三段写：A 基础与骨架、B 内容区块、C 进场门与特效层。
// 颜色类占位符（{{accent}} 等）由 C++ 在生成时替换，浅色主题的压暗变体也是
// C++ 按亮度算出来的——配色不由浏览器决定。
//
// 动效约定：页面自己的动画（关键帧）都由 html.motion-auto 统一开关，
// 只有配置成 auto 时才跟随系统的"减少动态效果"，默认 always 时完全独立。

#include "assets.hpp"

namespace ssg {

namespace {

// ---------- A. 变量、基础、背景层、左侧栏 ----------
const char* kCssBase = R"CSS(/* 由 C++ 生成器（src/web_style.cpp）产出，请不要直接改 dist/style.css */

:root {
  --accent: {{accent}};
  --accent2: {{accent2}};
  --accent3: {{accent3}};
  --on-accent: {{on_accent}};
  /* 未压暗的原色：首屏是深色封面，白字旁边要用亮色才好看 */
  --accent-hi: {{accent}};
  --accent2-hi: {{accent2}};
  --accent3-hi: {{accent3}};

  --bg: #06080f;
  --bg-soft: #0b0f1a;
  --panel: rgba(11, 16, 27, .74);
  --panel-border: rgba(255, 255, 255, .09);
  --text: #e9edf6;
  --muted: #8b94ab;
  --border: #1d2434;
  --card: rgba(18, 24, 38, .72);

  --side-w: 258px;
  --radius: 16px;
  --maxw: 1000px;

  --ease-out: cubic-bezier(.22, .61, .36, 1);
  --ease-soft: cubic-bezier(.33, 1, .68, 1);

  /* 壁纸：content/ 里有 wallpaper.* 才会被用上 */
  --wallpaper: {{wallpaper_css}};

  /* 像素火焰 / 光标粒子的调色板：canvas 从这里取色，浅色主题下自动换一套。
     火焰的 5 档色标按"底部最亮 → 顶部最暗"使用 */
  --fx-a: #d8f8ff;
  --fx-b: #7fdcff;
  --fx-c: #2e9bff;
  --fx-d: #12309a;
  --fx-e: #060f2c;
  --fx-opacity: .82;
  --cur-a: #9ef7ff;
  --cur-b: #ff2e8b;
  --cur-c: #ffd166;

  /* 进场门：暗灰底 + 橙色确认按钮（图 1 那种"封面页"的观感，与深浅主题无关） */
  --gate-bg: #3b3b3d;
  --gate-accent: {{gate_accent}};
  --gate-on-accent: {{gate_on_accent}};

  /* 芯片栏 */
  --chip-bg: #050b1c;
  --chip-line: #1c3f8f;
  --chip-pin: #3566d8;
  --chip-glow: #8fe3ff;
}

/* 浅色主题：强调色换成 C++ 算好的压暗版本，否则浅米色在白底上等于看不见 */
[data-theme="light"] {
  --bg: #f4f6fa;
  --bg-soft: #ffffff;
  --panel: rgba(255, 255, 255, .82);
  --panel-border: rgba(10, 16, 30, .08);
  --text: #131722;
  --muted: #5c6577;
  --border: #e2e7f0;
  --card: rgba(255, 255, 255, .9);
  --accent: {{accent_light}};
  --accent2: {{accent2_light}};
  --accent3: {{accent3_light}};
  --on-accent: {{on_accent_light}};
  --fx-opacity: .3;
  --fx-a: #dff1ff;
  --fx-b: #8fc7ff;
  --fx-c: #3f7ee8;
  --fx-d: #24479c;
  --fx-e: #dfe9fb;
  --cur-a: #2f7fd8;
  --cur-b: #e5487f;
  --cur-c: #d99a20;
  --chip-bg: #0a1226;
}

* { box-sizing: border-box; }

html {
  /* 平滑滚动由 site.js 自己算（不依赖 scroll-behavior），这里只保证不动画冲突 */
  scroll-behavior: auto;
  background: var(--bg);
  -webkit-text-size-adjust: 100%;
}

body {
  margin: 0;
  min-height: 100%;
  background: var(--bg);
  color: var(--text);
  font-family: system-ui, -apple-system, "Segoe UI", "PingFang SC",
               "Hiragino Sans GB", "Microsoft YaHei", Roboto, Helvetica, Arial, sans-serif;
  font-size: 16px;
  line-height: 1.6;
  overflow-x: hidden;
  -webkit-font-smoothing: antialiased;
}

body.nav-open { overflow: hidden; }

a { color: inherit; text-decoration: none; }
h1, h2, h3, h4 { line-height: 1.25; margin: 0 0 .6em; letter-spacing: -0.01em; }
p { margin: 0 0 1em; }
img { max-width: 100%; }

code {
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  font-size: .9em;
  background: var(--bg-soft);
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: .1em .4em;
}

:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; border-radius: 6px; }

::selection { background: color-mix(in srgb, var(--accent2) 40%, transparent); }

/* 滚动条：细一点、跟着主题 */
* { scrollbar-width: thin; scrollbar-color: var(--border) transparent; }
*::-webkit-scrollbar { width: 9px; height: 9px; }
*::-webkit-scrollbar-thumb { background: var(--border); border-radius: 9px; }
*::-webkit-scrollbar-track { background: transparent; }

/* ---------- 背景层：像素火焰与光标粒子 ---------- */

.fx-layer {
  position: fixed;
  inset: 0;
  pointer-events: none;
  z-index: 0;
}
.fx-layer canvas { display: block; width: 100%; height: 100%; }
#fx-fire { opacity: var(--fx-opacity); }
/* 前景粒子浮在内容之上，但永远不吃鼠标事件 */
.fx-front { z-index: 45; }

.shell { position: relative; z-index: 1; margin-left: var(--side-w); }

/* ---------- 顶部滚动进度（窄屏时侧栏收起，用它顶上） ---------- */

#progress {
  position: fixed;
  top: 0; left: 0; right: 0;
  height: 2px;
  z-index: 70;
  pointer-events: none;
  display: none;
}
#progress i {
  display: block;
  height: 100%;
  transform: scaleX(0);
  transform-origin: 0 50%;
  background: linear-gradient(90deg, var(--accent), var(--accent2), var(--accent3));
  box-shadow: 0 0 10px color-mix(in srgb, var(--accent) 60%, transparent);
}

/* ---------- 左侧竖排导航 ---------- */

.sidebar {
  position: fixed;
  left: 0; top: 0; bottom: 0;
  z-index: 30;
  width: var(--side-w);
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 20px 15px 16px;
  background: linear-gradient(180deg, rgba(9, 13, 21, .93), rgba(5, 8, 14, .88));
  border-right: 1px solid var(--border);
  backdrop-filter: blur(16px) saturate(1.2);
  -webkit-backdrop-filter: blur(16px) saturate(1.2);
  will-change: transform;
}
[data-theme="light"] .sidebar {
  background: linear-gradient(180deg, rgba(255, 255, 255, .96), rgba(244, 246, 250, .92));
}

/* 侧栏右缘的竖向进度条：滚动到哪一目了然 */
.side-rail {
  position: absolute;
  right: -1px; top: 0; bottom: 0;
  width: 2px;
  background: var(--border);
}
.side-rail b {
  display: block;
  width: 100%; height: 100%;
  transform: scaleY(0);
  transform-origin: 50% 0;
  background: linear-gradient(180deg, var(--accent), var(--accent2), var(--accent3));
}

.side-brand {
  display: flex;
  align-items: center;
  gap: 11px;
  padding: 7px 8px;
  border-radius: 12px;
}
.side-avatar {
  width: 46px; height: 46px;
  border-radius: 14px;
  object-fit: cover;
  border: 1px solid var(--panel-border);
  background: var(--bg-soft);
  flex: none;
}
.side-brand-txt { display: flex; flex-direction: column; line-height: 1.3; min-width: 0; }
.side-brand-txt b { font-size: 15px; font-weight: 650; }
.side-brand-txt i {
  font-style: normal;
  font-size: 11.5px;
  color: var(--muted);
  letter-spacing: .04em;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}

.side-search { position: relative; }
.side-search input {
  width: 100%;
  padding: 9px 32px 9px 11px;
  border-radius: 10px;
  border: 1px solid var(--border);
  background: color-mix(in srgb, var(--bg-soft) 70%, transparent);
  color: var(--text);
  font: inherit;
  font-size: 13.5px;
  outline: none;
  transition: border-color .2s, box-shadow .2s;
}
.side-search input::placeholder { color: color-mix(in srgb, var(--muted) 80%, transparent); }
.side-search input:focus {
  border-color: color-mix(in srgb, var(--accent) 60%, var(--border));
  box-shadow: 0 0 0 3px color-mix(in srgb, var(--accent) 15%, transparent);
}
.side-key {
  position: absolute;
  right: 9px; top: 50%;
  transform: translateY(-50%);
  font-size: 11px;
  color: var(--muted);
  border: 1px solid var(--border);
  border-radius: 5px;
  padding: 0 5px;
  pointer-events: none;
  transition: opacity .2s;
}
.side-search input:focus + .side-key { opacity: 0; }

/* 搜索结果 */
.side-hits {
  display: flex;
  flex-direction: column;
  gap: 3px;
  max-height: 38vh;
  overflow-y: auto;
  padding: 6px;
  border-radius: 10px;
  border: 1px solid var(--border);
  background: color-mix(in srgb, var(--bg-soft) 80%, transparent);
}
.side-hits[hidden] { display: none; }
.hit {
  display: flex;
  flex-direction: column;
  gap: 1px;
  padding: 7px 9px;
  border-radius: 8px;
  cursor: pointer;
  border: 0;
  background: transparent;
  color: inherit;
  font: inherit;
  text-align: left;
  width: 100%;
}
.hit:hover, .hit.is-cursor { background: color-mix(in srgb, var(--accent) 14%, transparent); }
.hit-k { font-size: 13px; color: var(--text); }
.hit-k mark { background: transparent; color: var(--accent); font-weight: 700; }
.hit-s { font-size: 11px; color: var(--muted); letter-spacing: .04em; }
.hit-empty { padding: 8px 9px; font-size: 12.5px; color: var(--muted); }

.side-nav {
  position: relative;
  display: flex;
  flex-direction: column;
  gap: 2px;
  margin-top: 2px;
}
/* 当前区块的滑块：位置由 site.js 逐帧补间，不用 CSS 过渡 */
.side-pill {
  position: absolute;
  left: 0; top: 0;
  width: 100%;
  height: 38px;
  border-radius: 10px;
  background: linear-gradient(90deg, color-mix(in srgb, var(--accent) 20%, transparent), transparent 85%);
  border-left: 2px solid var(--accent);
  pointer-events: none;
  will-change: transform, height;
}
.side-link {
  position: relative;
  z-index: 1;
  display: flex;
  align-items: baseline;
  gap: 10px;
  padding: 9px 11px;
  border-radius: 10px;
  color: var(--muted);
  font-size: 14px;
  transition: color .22s;
}
.side-link .n {
  font-size: 10.5px;
  letter-spacing: .08em;
  font-variant-numeric: tabular-nums;
  color: color-mix(in srgb, var(--accent) 65%, var(--muted));
}
.side-link .t { flex: 1; }
.side-link .c {
  font-size: 10.5px;
  font-variant-numeric: tabular-nums;
  color: color-mix(in srgb, var(--muted) 70%, transparent);
}
.side-link:hover { color: var(--text); }
.side-link.active { color: var(--text); font-weight: 600; }
.side-link.active .n { color: var(--accent); }

.side-foot {
  margin-top: auto;
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 7px;
  padding-top: 12px;
  border-top: 1px solid var(--border);
}
#theme-btn {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 6px 9px;
  border: 1px solid var(--border);
  border-radius: 9px;
  background: transparent;
  color: var(--muted);
  font: inherit;
  font-size: 12.5px;
  cursor: pointer;
  transition: color .2s, border-color .2s;
}
#theme-btn:hover { color: var(--text); border-color: var(--accent); }
#theme-btn em { font-style: normal; }
.side-foot a {
  padding: 6px 9px;
  border-radius: 9px;
  font-size: 12.5px;
  color: var(--muted);
  transition: color .2s, background .2s;
}
.side-foot a:hover { color: var(--text); background: color-mix(in srgb, var(--accent) 12%, transparent); }
.side-meta {
  width: 100%;
  font-size: 11px;
  color: color-mix(in srgb, var(--muted) 75%, transparent);
  letter-spacing: .04em;
}
.side-meta .blink { animation: blink 1.1s steps(2, end) infinite; color: var(--accent3); }
@keyframes blink { 0%, 100% { opacity: 1 } 50% { opacity: 0 } }

/* ---------- 窄屏：侧栏变抽屉 ---------- */

.nav-toggle {
  display: none;
  position: fixed;
  left: 12px; top: 12px;
  z-index: 60;
  width: 42px; height: 42px;
  padding: 0;
  border: 1px solid var(--panel-border);
  border-radius: 12px;
  background: color-mix(in srgb, var(--bg) 78%, transparent);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  cursor: pointer;
}
.nav-toggle i {
  display: block;
  width: 18px; height: 2px;
  margin: 3px auto;
  border-radius: 2px;
  background: var(--text);
  transition: transform .28s var(--ease-out), opacity .2s;
}
body.nav-open .nav-toggle i:nth-child(1) { transform: translateY(5px) rotate(45deg); }
body.nav-open .nav-toggle i:nth-child(2) { opacity: 0; }
body.nav-open .nav-toggle i:nth-child(3) { transform: translateY(-5px) rotate(-45deg); }

.nav-scrim {
  position: fixed;
  inset: 0;
  z-index: 29;
  background: rgba(2, 4, 9, .55);
  opacity: 0;
  transition: opacity .3s var(--ease-out);
}
.nav-scrim[hidden] { display: none; }
.nav-scrim.on { opacity: 1; }
)CSS";

// ---------- B. 首屏、内容区块、芯片栏、页脚 ----------
const char* kCssBody = R"CSS(/* ---------- 首屏：上方壁纸，向下淡入深色 ---------- */

.hero {
  position: relative;
  isolation: isolate;
  overflow: hidden;
  display: grid;
  grid-template-columns: 132px 1fr;
  gap: clamp(20px, 3.4vw, 44px);
  align-items: center;
  min-height: min(74vh, 640px);
  padding: clamp(58px, 8vw, 104px) clamp(22px, 4.6vw, 68px) clamp(48px, 6vw, 76px);
}

/* 壁纸本体（有 content/wallpaper.* 时才有图） */
.hero::before {
  content: "";
  position: absolute;
  inset: 0;
  z-index: -2;
  background-image: var(--wallpaper);
  background-size: cover;
  background-position: center 44%;
  background-repeat: no-repeat;
  transform: translate(var(--px, 0px), var(--py, 0px)) scale(1.05);
}

/* 遮罩：上面压暗保住文字对比度，下面过渡到深色背景，让"上半张壁纸 + 下半深色"接得自然 */
.hero::after {
  content: "";
  position: absolute;
  inset: 0;
  z-index: -1;
  pointer-events: none;
  background:
    linear-gradient(180deg,
      color-mix(in srgb, var(--bg) 72%, transparent) 0%,
      color-mix(in srgb, var(--bg) 24%, transparent) 38%,
      color-mix(in srgb, var(--bg) 58%, transparent) 74%,
      var(--bg) 100%),
    linear-gradient(90deg,
      color-mix(in srgb, var(--bg) 84%, transparent) 0%,
      color-mix(in srgb, var(--bg) 40%, transparent) 46%,
      transparent 78%);
}

/* 浅色主题下首屏仍然保持"深色封面"的观感：
   白字压在浅色遮罩上会糊成一片，所以这里的遮罩改用暗色，
   只在最底下一行过渡回浅色背景（下面接的就是白底正文）。 */
[data-theme="light"] .hero::after {
  background:
    linear-gradient(180deg,
      rgba(8, 10, 16, .6) 0%,
      rgba(8, 10, 16, .16) 38%,
      rgba(8, 10, 16, .52) 74%,
      var(--bg) 100%),
    linear-gradient(90deg,
      rgba(8, 10, 16, .7) 0%,
      rgba(8, 10, 16, .28) 46%,
      transparent 78%);
}
[data-theme="light"] .hero .btn {
  background: rgba(10, 14, 22, .5);
  border-color: rgba(255, 255, 255, .34);
  color: #fff;
}
[data-theme="light"] .hero .btn-primary {
  border-color: transparent;
  color: var(--on-accent);
  background: linear-gradient(135deg, var(--accent), var(--accent2));
}
[data-theme="light"] .avatar {
  border-color: rgba(255, 255, 255, .6);
}

.hero-avatar { position: relative; }

.hero-avatar::after {
  content: "";
  position: absolute;
  inset: -22%;
  z-index: -1;
  border-radius: 50%;
  background: radial-gradient(circle, color-mix(in srgb, var(--accent3) 46%, transparent), transparent 70%);
  filter: blur(15px);
  animation: avatarGlow 5.5s ease-in-out infinite;
}
.avatar {
  width: 132px; height: 132px;
  border-radius: 26px;
  object-fit: cover;
  border: 3px solid rgba(255, 255, 255, .5);
  box-shadow: 0 16px 46px rgba(0, 0, 0, .5);
  animation: avatarFloat 7s ease-in-out infinite;
}
@keyframes avatarFloat {
  0%, 100% { transform: translateY(0) }
  50%      { transform: translateY(-8px) }
}
@keyframes avatarGlow {
  0%, 100% { opacity: .4; transform: scale(.96) }
  50%      { opacity: .85; transform: scale(1.06) }
}

.hero-text { position: relative; text-shadow: 0 2px 18px rgba(0, 0, 0, .55); }
.eyebrow {
  margin: 0 0 9px;
  font-size: 12px;
  letter-spacing: .2em;
  text-transform: uppercase;
  color: #fff;
  opacity: .92;
}
.hero h1 {
  margin: 0;
  font-size: clamp(38px, 7vw, 62px);
  font-weight: 700;
  letter-spacing: -0.03em;
  color: #fff;
  min-height: 1.15em;
}
.hero h1 .grad {
  background: linear-gradient(120deg, #fff 20%, var(--accent-hi) 75%, var(--accent2-hi));
  background-size: 220% 100%;
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent;
  animation: nameFlow 9s ease-in-out infinite;
}
@keyframes nameFlow {
  0%, 100% { background-position: 0% 50% }
  50%      { background-position: 100% 50% }
}
.type-caret {
  display: inline-block;
  width: 3px;
  height: .8em;
  margin-left: .06em;
  vertical-align: -.05em;
  border-radius: 2px;
  background: linear-gradient(180deg, var(--accent-hi), var(--accent2-hi));
  animation: blink .8s steps(2, end) infinite;
}
.type-caret.hide { opacity: 0; transition: opacity .5s var(--ease-out); }

.role { margin: 10px 0 0; font-size: 19px; font-weight: 600; color: #fff; }
.tagline { margin: 8px 0 0; max-width: 48ch; font-size: 16px; color: rgba(255, 255, 255, .88); }

.cta { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 24px; }
.btn {
  position: relative;
  overflow: hidden;
  display: inline-block;
  padding: 10px 18px;
  border: 1px solid rgba(255, 255, 255, .3);
  border-radius: 10px;
  background: color-mix(in srgb, var(--bg) 46%, transparent);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
  color: #fff;
  font-size: 14px;
  font-weight: 600;
  transition: transform .18s, border-color .18s, box-shadow .18s;
}
.btn:hover { transform: translateY(-2px); border-color: var(--accent); }
.btn-primary {
  border-color: transparent;
  color: var(--on-accent);
  background: linear-gradient(135deg, var(--accent), var(--accent2));
  box-shadow: 0 10px 28px -12px color-mix(in srgb, var(--accent) 80%, transparent);
}
/* 悬停时一道高光扫过（关键帧在 C 段统一开关） */
.btn::after {
  content: "";
  position: absolute;
  top: 0; bottom: 0; left: -70%;
  width: 45%;
  background: linear-gradient(100deg, transparent, rgba(255, 255, 255, .38), transparent);
  transform: skewX(-18deg);
  opacity: 0;
  pointer-events: none;
}
.btn:hover::after { animation: btnSweep .75s var(--ease-soft); }
@keyframes btnSweep {
  0%   { left: -70%; opacity: 1 }
  100% { left: 130%; opacity: 0 }
}

.hero-stats {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 18px;
  list-style: none;
  margin: 26px 0 0;
  padding: 0;
}
.hero-stats li {
  display: flex;
  align-items: baseline;
  gap: 6px;
  font-size: 12.5px;
  color: rgba(255, 255, 255, .82);
  letter-spacing: .04em;
}
.hero-stats b {
  font-size: 18px;
  font-variant-numeric: tabular-nums;
  color: #fff;
}

/* 首屏一道缓慢扫过的扫描线，给静态壁纸加"在线"感 */
.hero-scan {
  position: absolute;
  left: 0; right: 0;
  height: 160px;
  top: -160px;
  z-index: 0;
  pointer-events: none;
  background: linear-gradient(180deg, transparent, color-mix(in srgb, var(--accent3-hi) 16%, transparent), transparent);
  animation: heroScan 7.5s linear infinite;
}
@keyframes heroScan {
  0%   { top: -160px }
  100% { top: 100% }
}

/* ---------- 内容区块：磨砂面板 ---------- */

main { padding: 0 clamp(14px, 2.4vw, 30px); }

.block {
  position: relative;
  max-width: var(--maxw);
  margin: clamp(16px, 2.2vw, 28px) auto;
  padding: clamp(26px, 3.6vw, 46px) clamp(20px, 3.4vw, 44px);
  border: 1px solid var(--panel-border);
  border-radius: 20px;
  background: var(--panel);
  backdrop-filter: blur(15px) saturate(1.25);
  -webkit-backdrop-filter: blur(15px) saturate(1.25);
  box-shadow: 0 34px 80px -46px rgba(0, 0, 0, .95);
}
.block::before {
  content: "";
  position: absolute;
  left: 8%; right: 8%; top: -1px;
  height: 1px;
  background: linear-gradient(90deg, transparent,
              color-mix(in srgb, var(--accent) 55%, transparent), transparent);
  opacity: .7;
}

.block h2 {
  display: flex;
  align-items: baseline;
  gap: 12px;
  font-size: 23px;
  margin-bottom: 22px;
}
.block h2 .num {
  position: relative;
  font-size: 12.5px;
  font-weight: 700;
  letter-spacing: .06em;
  font-variant-numeric: tabular-nums;
  color: var(--accent);
}
.block h2 .num::after {
  content: "";
  position: absolute;
  left: 0; right: 0; bottom: -5px;
  height: 2px;
  transform: scaleX(0);
  transform-origin: left;
  background: linear-gradient(90deg, var(--accent), var(--accent2));
}
/* 区块进场完成后由 site.js 挂上 .in，数字下划线才展开 */
.block.in h2 .num::after { transform: scaleX(1); transition: transform .7s var(--ease-out) .18s; }

.block-lead { color: var(--muted); font-size: 14px; margin: -12px 0 20px; }

/* ---------- 关于我：对话气泡 ---------- */

.prose p { color: var(--muted); line-height: 1.85; max-width: 62ch; }
.prose strong { color: var(--text); }
.prose a { color: var(--accent); border-bottom: 1px solid transparent; }
.prose a:hover { border-bottom-color: var(--accent); }
.prose h3 { font-size: 18px; margin-top: 1.6em; color: var(--text); }
.prose hr { border: 0; border-top: 1px solid var(--border); margin: 30px 0; }

.dialogue { display: flex; flex-direction: column; gap: 13px; margin: 4px 0 8px; }

.dlg {
  position: relative;
  max-width: 94%;
  padding: 12px 18px 13px 58px;
  border: 1px solid var(--border);
  border-radius: 14px 14px 14px 4px;
  background: color-mix(in srgb, var(--bg-soft) 78%, transparent);
  transition: border-color .25s, transform .25s;
}
.dlg:hover {
  border-color: color-mix(in srgb, var(--accent) 45%, var(--border));
  transform: translateX(3px);
}
.dlg::after {
  content: "";
  position: absolute;
  left: -1px; bottom: -1px;
  width: 12px; height: 12px;
  background: color-mix(in srgb, var(--bg-soft) 78%, transparent);
  border-left: 1px solid var(--border);
  border-bottom: 1px solid var(--border);
  border-bottom-left-radius: 14px;
}
.dlg-who {
  position: absolute;
  left: 14px; top: 13px;
  width: 32px; height: 32px;
  display: grid;
  place-items: center;
  border-radius: 50%;
  font-size: 15px;
  font-weight: 700;
  color: var(--on-accent);
  background: linear-gradient(135deg, var(--accent), var(--accent2));
  box-shadow: 0 3px 10px -3px color-mix(in srgb, var(--accent) 60%, transparent);
  user-select: none;
  transition: transform .25s var(--ease-out);
}
.dlg:hover .dlg-who { transform: scale(1.07); }
.dlg-text { margin: 0; color: var(--text); font-size: 15px; line-height: 1.75; }
.dlg-text strong { color: var(--accent); }
.dlg-text a { color: var(--accent); }

/* 关于我最下面的"自评条"：about.md 末尾写「- 名称 | 百分比」就会出现在这里 */
.about-bars {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(212px, 1fr));
  gap: 10px;
  margin-top: 24px;
  padding-top: 20px;
  border-top: 1px dashed color-mix(in srgb, var(--accent) 32%, var(--border));
}

/* 搜索命中时闪一下 */
.flash { animation: flashHit 1.5s var(--ease-out); }
@keyframes flashHit {
  0%   { box-shadow: 0 0 0 0 color-mix(in srgb, var(--accent2) 70%, transparent) }
  30%  { box-shadow: 0 0 0 6px color-mix(in srgb, var(--accent2) 26%, transparent) }
  100% { box-shadow: 0 0 0 0 transparent }
}

/* ---------- 技能：大框（分组）套小框（每个技能） ---------- */

.skill-group {
  margin-bottom: 16px;
  padding: 16px 18px 18px;
  border: 1px solid color-mix(in srgb, var(--accent) 22%, var(--border));
  border-radius: 16px;
  background: color-mix(in srgb, var(--bg-soft) 55%, transparent);
}
.skill-group:last-child { margin-bottom: 0; }

.skill-group-head {
  display: flex;
  align-items: baseline;
  gap: 10px;
  margin-bottom: 13px;
}
.skill-group-head h3 {
  font-size: 13px;
  color: var(--text);
  font-weight: 700;
  letter-spacing: .1em;
  text-transform: uppercase;
  margin: 0;
}
.skill-group-head h3::before {
  content: "";
  display: inline-block;
  width: 7px; height: 7px;
  margin-right: 8px;
  border-radius: 2px;
  background: linear-gradient(135deg, var(--accent), var(--accent2));
  vertical-align: 1px;
}
.skill-group-n {
  margin-left: auto;
  font-size: 11px;
  letter-spacing: .06em;
  color: var(--muted);
  font-variant-numeric: tabular-nums;
}

.skill-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(212px, 1fr));
  gap: 10px;
}

/* 每个技能一个小框：进度条就长在框里 */
.skill {
  padding: 11px 13px 12px;
  border: 1px solid var(--border);
  border-radius: 12px;
  background: var(--card);
  transition: transform .2s var(--ease-out), border-color .2s, box-shadow .2s;
}
.skill:hover {
  transform: translateY(-2px);
  border-color: color-mix(in srgb, var(--accent) 55%, var(--border));
  box-shadow: 0 12px 26px -16px rgba(0, 0, 0, .75);
}

.skill-head {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  gap: 10px;
  font-size: 13.5px;
  margin-bottom: 9px;
}
.skill-name {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.skill-pct {
  color: var(--accent);
  font-weight: 700;
  font-variant-numeric: tabular-nums;
}
.skill-track {
  height: 6px;
  border-radius: 999px;
  background: color-mix(in srgb, var(--bg) 70%, transparent);
  border: 1px solid var(--border);
  overflow: hidden;
}
.skill-fill {
  position: relative;
  overflow: hidden;
  height: 100%;
  width: 0;
  border-radius: 999px;
  background: linear-gradient(90deg, var(--accent), var(--accent2));
}
.skill-fill::after {
  content: "";
  position: absolute;
  inset: 0;
  background: linear-gradient(100deg, transparent 20%, rgba(255, 255, 255, .42) 50%, transparent 80%);
  transform: translateX(-120%);
  animation: barSweep 2.8s var(--ease-soft) infinite;
}
@keyframes barSweep {
  0%        { transform: translateX(-120%) }
  55%, 100% { transform: translateX(220%) }
}

/* ---------- 项目卡 ---------- */

.cards {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(290px, 1fr));
  gap: 15px;
}
.card {
  position: relative;
  overflow: hidden;
  padding: 21px 21px 19px;
  border: 1px solid var(--border);
  border-radius: var(--radius);
  background: var(--card);
  transition: transform .2s, border-color .2s, box-shadow .2s;
}
.card:hover {
  transform: translateY(-4px);
  border-color: color-mix(in srgb, var(--accent) 55%, var(--border));
  box-shadow: 0 16px 38px -18px rgba(0, 0, 0, .7),
              0 0 0 1px color-mix(in srgb, var(--accent2) 26%, transparent);
}
.card::before {
  content: "";
  position: absolute;
  inset: 0;
  background: radial-gradient(240px circle at var(--mx, 50%) var(--my, 50%),
              color-mix(in srgb, var(--accent2) 16%, transparent), transparent 68%);
  opacity: 0;
  transition: opacity .35s var(--ease-out);
  pointer-events: none;
}
.card:hover::before { opacity: 1; }
.card h3 { font-size: 17px; margin-bottom: 8px; }
.card p { color: var(--muted); font-size: 14.5px; margin-bottom: .7em; }
.card p:last-child { margin-bottom: 0; }
.card ul {
  list-style: none;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin: 14px 0 0;
  padding: 0;
}
.card ul li a {
  display: inline-block;
  padding: 4px 11px;
  border: 1px solid var(--border);
  border-radius: 999px;
  font-size: 12.5px;
  color: var(--muted);
  transition: color .18s, border-color .18s;
}
.card ul li a:hover { color: var(--accent); border-color: var(--accent); }

/* ---------- 经历 ---------- */

.timeline {
  list-style: none;
  margin: 0;
  padding: 2px 0 0 26px;
  border-left: 2px solid var(--border);
}
.timeline li { position: relative; padding-bottom: 25px; }
.timeline li:last-child { padding-bottom: 0; }
.timeline li::before {
  content: "";
  position: absolute;
  left: -33px; top: 5px;
  width: 12px; height: 12px;
  border-radius: 50%;
  background: linear-gradient(135deg, var(--accent), var(--accent2));
  box-shadow: 0 0 0 3px var(--bg);
}
.t-when {
  font-size: 12.5px;
  letter-spacing: .08em;
  color: var(--accent);
  font-variant-numeric: tabular-nums;
  margin-bottom: 3px;
}
.t-body h4 { font-size: 16px; margin: 0 0 4px; font-weight: 600; }
.t-body p { color: var(--muted); font-size: 14.5px; margin: 0; }

/* ---------- 核心面板：芯片 + 四周电流 ---------- */

.board {
  position: relative;
  overflow: hidden;
  border-radius: 16px;
  border: 1px solid color-mix(in srgb, var(--chip-line) 60%, transparent);
  background:
    radial-gradient(120% 130% at 50% 50%, color-mix(in srgb, var(--chip-bg) 92%, #0a1738) 0%, var(--chip-bg) 62%, #02040a 100%);
  min-height: clamp(300px, 40vw, 396px);
  box-shadow: inset 0 0 70px -20px color-mix(in srgb, var(--chip-line) 70%, transparent);
}
.board canvas { position: absolute; inset: 0; width: 100%; height: 100%; }

.board-hud {
  position: absolute;
  left: clamp(14px, 2.2vw, 24px);
  top: clamp(14px, 2.2vw, 24px);
  display: flex;
  flex-direction: column;
  gap: 7px;
  max-width: min(52%, 340px);
  padding: 11px 14px;
  border: 1px solid color-mix(in srgb, var(--chip-line) 55%, transparent);
  border-radius: 10px;
  background: rgba(4, 9, 20, .62);
  backdrop-filter: blur(5px);
  -webkit-backdrop-filter: blur(5px);
  pointer-events: none;
}
.spec { display: flex; align-items: baseline; gap: 9px; font-size: 12.5px; }
.spec i {
  font-style: normal;
  font-size: 10px;
  letter-spacing: .12em;
  text-transform: uppercase;
  color: color-mix(in srgb, var(--chip-glow) 62%, var(--muted));
  min-width: 54px;
}
.spec b { font-weight: 600; color: var(--text); word-break: break-all; }
.spec b a { color: var(--chip-glow); }

.board-corner {
  position: absolute;
  width: 14px; height: 14px;
  border: 2px solid color-mix(in srgb, var(--chip-glow) 70%, transparent);
  opacity: .75;
}
.board-corner.tl { left: 9px; top: 9px; border-right: 0; border-bottom: 0; }
.board-corner.tr { right: 9px; top: 9px; border-left: 0; border-bottom: 0; }
.board-corner.bl { left: 9px; bottom: 9px; border-right: 0; border-top: 0; }
.board-corner.br { right: 9px; bottom: 9px; border-left: 0; border-top: 0; }

/* ---------- 页脚 ---------- */

footer {
  border-top: 1px solid var(--border);
  margin-top: 34px;
  background: color-mix(in srgb, var(--bg-soft) 62%, transparent);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
}
.footer-inner {
  max-width: var(--maxw);
  margin: 0 auto;
  padding: 46px clamp(20px, 3vw, 40px);
  text-align: center;
}
.footer-cta { font-size: 19px; font-weight: 600; margin-bottom: 13px; }
.footer-cta a { color: var(--accent); border-bottom: 1px solid transparent; }
.footer-cta a:hover { border-bottom-color: var(--accent); }
.footer-note { color: var(--muted); font-size: 13px; margin: 0 0 4px; }
)CSS";

// ---------- C. 进场门、WELCOME、动效开关、响应式、打印 ----------
const char* kCssGate = R"CSS(/* ============================================================
   进场门：仿图 1 的"封面页"——暗灰底、海报轮播、圆点、橙色确认按钮。
   门只是浮在正文之上的一层覆盖，正文始终完整存在于 HTML 里，
   所以搜索引擎和链接分享的预览照样能抓到内容；JS 不可用时门不显示。
   ============================================================ */

.gate-on .shell,
.gate-on .sidebar,
.gate-on .nav-toggle,
.gate-on #progress { visibility: hidden; }

#gate {
  display: none;
  position: fixed;
  inset: 0;
  z-index: 80;
  overflow-y: auto;
  overscroll-behavior: contain;
  background: var(--gate-bg);
  color: #f2f2f2;
}
.gate-on #gate { display: block; }

/* 暗底上的细网格，缓慢漂移 */
#gate::before {
  content: "";
  position: fixed;
  inset: -50px;
  pointer-events: none;
  opacity: .5;
  background-image:
    linear-gradient(rgba(255, 255, 255, .05) 1px, transparent 1px),
    linear-gradient(90deg, rgba(255, 255, 255, .05) 1px, transparent 1px);
  background-size: 46px 46px;
  mask-image: radial-gradient(60% 60% at 50% 45%, #000 20%, transparent 74%);
  -webkit-mask-image: radial-gradient(60% 60% at 50% 45%, #000 20%, transparent 74%);
  animation: drift 22s linear infinite;
}
@keyframes drift { to { background-position: 46px 46px, 46px 46px } }

.gate-stage {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 100%;
  width: min(520px, 100%);
  margin: 0 auto;
  padding: clamp(18px, 4vh, 44px) 18px calc(30px + env(safe-area-inset-bottom));
  text-align: center;
}

/* ---- 轮播 ---- */
.gate-slides {
  position: relative;
  width: 100%;
  aspect-ratio: 4 / 5;
  max-height: 62vh;
  border-radius: 10px;
  overflow: hidden;
  background: #15161a;
  box-shadow: 0 26px 70px -22px rgba(0, 0, 0, .85);
}
.gate-slide {
  position: absolute;
  inset: 0;
  margin: 0;
  opacity: 0;
  will-change: opacity, transform;
}
.gate-slide.is-on { opacity: 1; }

/* 封面那一屏：上面是海报，下面是像素横条，两块各占各的位置，不互相盖 */
.gate-slide.is-cover { display: flex; flex-direction: column; }
.gate-slide.is-cover .gate-poster { position: relative; flex: 1 1 auto; min-height: 0; }

.gate-poster { position: absolute; inset: 0; }
.gate-poster img { width: 100%; height: 100%; object-fit: cover; display: block; }
.gate-poster::after {
  content: "";
  position: absolute;
  inset: 0;
  background: linear-gradient(180deg, rgba(0, 0, 0, .34) 0%, transparent 34%, rgba(0, 0, 0, .58) 100%);
}

/* 欢迎牌：白底粉字，压在封面下缘（图 1 的那块牌子）。
   字用实色 + 8 向白色描边，不用 background-clip:text —— 那个在部分浏览器上
   会和 text-shadow 打架，字会整块消失。 */
.gate-plaque {
  position: absolute;
  left: 50%;
  bottom: 8%;
  z-index: 2;
  transform: translateX(-50%);
  max-width: 92%;
  padding: 6px 26px 10px;
  border-radius: 16px;
  background: #fff;
  border: 4px solid #ffd7e5;
  box-shadow: 0 14px 34px -10px rgba(255, 45, 111, .55);
}
.gate-plaque i {
  display: block;
  font-style: normal;
  font-size: 12.5px;
  font-weight: 800;
  letter-spacing: .34em;
  color: #ff2d6f;
  text-indent: .34em;
}
.gate-plaque b {
  display: block;
  font-size: clamp(26px, 8vw, 42px);
  font-weight: 900;
  letter-spacing: .02em;
  line-height: 1.2;
  color: #ff2d6f;
  text-shadow:
    -3px -3px 0 #fff, 3px -3px 0 #fff, -3px 3px 0 #fff, 3px 3px 0 #fff,
    0 -3px 0 #fff, 0 3px 0 #fff, -3px 0 0 #fff, 3px 0 0 #fff,
    0 5px 0 #ffc2d8, 0 8px 16px rgba(255, 45, 111, .35);
}
/* 牌子左右两朵像素小花 */
.gate-plaque::before,
.gate-plaque::after {
  content: "";
  position: absolute;
  top: -12px;
  width: 26px; height: 26px;
  background:
    radial-gradient(circle at 50% 50%, #ffe9f2 0 26%, transparent 27%),
    radial-gradient(circle at 50% 12%, #ff86ac 0 26%, transparent 27%),
    radial-gradient(circle at 12% 50%, #ff86ac 0 26%, transparent 27%),
    radial-gradient(circle at 88% 50%, #ff86ac 0 26%, transparent 27%),
    radial-gradient(circle at 50% 88%, #ff86ac 0 26%, transparent 27%);
}
.gate-plaque::before { left: -14px }
.gate-plaque::after { right: -14px }

/* 封面下方的横条：像素吉祥物 + 标题（图 1 的 "禁漫發佈頁" 那条）。
   它是封面的兄弟节点而不是浮在上面，所以永远不挡牌子。 */
.gate-strip {
  position: relative;
  flex: none;
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 7px 12px;
  border-radius: 0 0 9px 9px;
  background: linear-gradient(90deg, rgba(28, 32, 42, .96), rgba(16, 18, 24, .94));
  border-top: 1px solid rgba(255, 255, 255, .12);
}
/* 像素小人的画布不设尺寸：用位图自己的大小，保证每一格都是整数像素。
   背景已经在生成器里裁掉了，所以这里用 drop-shadow（跟着图形轮廓走），
   而不是 box-shadow（会画出一个方块影子） */
.gate-strip canvas {
  flex: none;
  filter: drop-shadow(0 3px 7px rgba(0, 0, 0, .75));
  image-rendering: pixelated;
  image-rendering: crisp-edges;
}
.gate-strip b {
  font-size: clamp(15px, 4.2vw, 21px);
  font-weight: 900;
  letter-spacing: .05em;
  color: #ffb03a;
  text-shadow: 2px 2px 0 #7a3200, 4px 4px 0 rgba(0, 0, 0, .4);
}
.gate-strip span {
  margin-left: auto;
  font-size: 10.5px;
  letter-spacing: .18em;
  color: rgba(255, 255, 255, .55);
}

/* ---- 其余几屏：卡片式排版 ---- */
.gate-card {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 14px;
  padding: clamp(22px, 5vw, 40px);
  background:
    radial-gradient(90% 70% at 50% 0%, rgba(255, 176, 58, .12), transparent 62%),
    linear-gradient(160deg, #23262e, #14161c);
}
.gate-face {
  width: clamp(112px, 30vw, 158px);
  height: clamp(112px, 30vw, 158px);
  border-radius: 22px;
  object-fit: cover;
  border: 3px solid rgba(255, 255, 255, .32);
  box-shadow: 0 16px 42px rgba(0, 0, 0, .55);
}
.gate-card h3 {
  margin: 0;
  font-size: clamp(20px, 5.4vw, 27px);
  font-weight: 800;
  letter-spacing: -0.01em;
}
.gate-card p { margin: 0; font-size: 13.5px; line-height: 1.75; color: rgba(255, 255, 255, .72); max-width: 34ch; }
.gate-mini {
  display: flex;
  flex-direction: column;
  gap: 9px;
  width: 100%;
  max-width: 360px;
  text-align: left;
}
.gate-mini div {
  padding: 10px 13px;
  border-radius: 10px;
  border: 1px solid rgba(255, 255, 255, .1);
  background: rgba(255, 255, 255, .045);
  font-size: 12.8px;
  line-height: 1.6;
  color: rgba(255, 255, 255, .78);
}
.gate-mini span {
  display: block;
  font-size: 10.5px;
  letter-spacing: .16em;
  text-transform: uppercase;
  color: #ffb03a;
  margin-bottom: 3px;
}
.gate-contact { display: flex; flex-direction: column; gap: 8px; font-size: 13.5px; }
.gate-contact a { color: #ffd48a; border-bottom: 1px solid rgba(255, 212, 138, .35); }

/* ---- 圆点 + 按钮 ---- */
.gate-dots {
  display: flex;
  justify-content: center;
  gap: 9px;
  margin: 16px 0 15px;
}
.gate-dots button {
  width: 8px; height: 8px;
  padding: 0;
  border: 0;
  border-radius: 50%;
  background: rgba(255, 255, 255, .3);
  cursor: pointer;
  transition: background .25s, transform .25s;
}
.gate-dots button:hover { background: rgba(255, 255, 255, .55); }
.gate-dots button.on { background: var(--gate-accent); transform: scale(1.25); }

#gate-btn {
  position: relative;
  overflow: hidden;
  width: 100%;
  max-width: 480px;
  padding: 16px 22px;
  border: 0;
  border-radius: 8px;
  background: var(--gate-accent);
  color: var(--gate-on-accent);
  font: inherit;
  font-size: 16px;
  font-weight: 700;
  letter-spacing: .02em;
  cursor: pointer;
  box-shadow: 0 14px 34px -14px color-mix(in srgb, var(--gate-accent) 85%, transparent);
  transition: transform .18s, filter .2s;
}
#gate-btn:hover { transform: translateY(-2px); filter: brightness(1.06); }
#gate-btn:active { transform: translateY(0); }
.gate-btn-label { position: relative; z-index: 2; }
.gate-btn-fill {
  position: absolute;
  top: 0; bottom: 0; left: -60%;
  width: 45%;
  z-index: 1;
  background: linear-gradient(100deg, transparent, rgba(255, 255, 255, .6), transparent);
  transform: skewX(-18deg);
}
#gate-btn.pressed .gate-btn-fill { animation: sweep .5s ease-out; }
@keyframes sweep { to { left: 125% } }

.gate-note { margin: 13px 0 0; font-size: 12.5px; color: rgba(255, 255, 255, .5); }

.gate-links {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 9px;
  width: 100%;
  max-width: 480px;
  margin-top: 26px;
}
.gate-links a {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 46px;
  padding: 9px 7px;
  border: 1px solid rgba(255, 255, 255, .14);
  border-radius: 9px;
  background: rgba(255, 255, 255, .05);
  font-size: 13px;
  font-weight: 600;
  text-align: center;
  transition: transform .16s, border-color .2s, background .2s, color .2s;
}
.gate-links a:hover {
  transform: translateY(-2px);
  border-color: var(--gate-accent);
  color: #ffd48a;
  background: rgba(255, 176, 58, .1);
}

#gate.out { animation: gateOut .42s ease forwards; }
@keyframes gateOut { to { opacity: 0; visibility: hidden } }

/* ============================================================
   WELCOME：进门后的赛博像素加载动画
   文字由 fx.js 在低分辨率画布上画完再整数倍放大（真方块像素），
   这里负责底噪：网格、扫描线、辉光、进度条与启动日志。
   ============================================================ */

#welcome {
  position: fixed;
  inset: 0;
  z-index: 90;
  display: none;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: #04060a;
  overflow: hidden;
}
#welcome.on  { display: flex; animation: wIn .22s ease both; }
#welcome.out { animation: wOut .46s ease forwards; }
@keyframes wIn  { from { opacity: 0 } to { opacity: 1 } }
@keyframes wOut { to { opacity: 0; visibility: hidden } }

#welcome::before {
  content: "";
  position: absolute;
  inset: -60px;
  pointer-events: none;
  background-image:
    linear-gradient(rgba(1, 122, 128, .16) 1px, transparent 1px),
    linear-gradient(90deg, rgba(1, 122, 128, .16) 1px, transparent 1px);
  background-size: 42px 42px;
  mask-image: radial-gradient(58% 58% at 50% 46%, #000 20%, transparent 76%);
  -webkit-mask-image: radial-gradient(58% 58% at 50% 46%, #000 20%, transparent 76%);
  animation: wGrid 18s linear infinite;
}
@keyframes wGrid { to { background-position: 42px 42px } }

#welcome::after {
  content: "";
  position: absolute;
  inset: 0;
  pointer-events: none;
  background:
    radial-gradient(52% 42% at 22% 28%, rgba(1, 122, 128, .3), transparent 68%),
    radial-gradient(52% 42% at 78% 72%, rgba(255, 61, 154, .22), transparent 68%),
    radial-gradient(120% 100% at 50% 50%, transparent 42%, rgba(0, 0, 0, .82) 100%);
}

#welcome-canvas {
  position: relative;
  z-index: 1;
  image-rendering: pixelated;
  image-rendering: crisp-edges;
  filter: drop-shadow(0 0 12px rgba(1, 122, 128, .9))
          drop-shadow(0 0 34px rgba(1, 122, 128, .45));
}

.w-scan {
  position: absolute;
  left: 0; right: 0;
  height: 140px;
  z-index: 2;
  pointer-events: none;
  background: linear-gradient(180deg, transparent, rgba(158, 247, 255, .1), transparent);
  animation: wScan 2.4s linear infinite;
}
@keyframes wScan {
  from { top: -140px }
  to   { top: 100% }
}

.w-hud {
  position: relative;
  z-index: 1;
  margin-top: clamp(18px, 4vh, 40px);
  width: min(430px, 76vw);
  text-align: center;
}
.w-bar {
  height: 8px;
  background: rgba(158, 247, 255, .1);
  border: 1px solid rgba(158, 247, 255, .22);
  overflow: hidden;
}
.w-bar i {
  display: block;
  height: 100%;
  width: 0;
  background: repeating-linear-gradient(90deg, #9ef7ff 0 6px, rgba(158, 247, 255, .25) 6px 10px);
  box-shadow: 0 0 12px rgba(1, 122, 128, .85);
}
.w-log {
  margin-top: 14px;
  min-height: 4.6em;
  font-family: "Cascadia Mono", Consolas, "Courier New", monospace;
  font-size: 11.5px;
  line-height: 1.55;
  letter-spacing: .08em;
  text-align: left;
  white-space: pre-line;
  color: #5fd9e0;
  text-shadow: 0 0 8px rgba(1, 122, 128, .75);
}

/* ============================================================
   动效开关
   页面自己写的关键帧动画默认全部播放（不依赖系统的"减少动态效果"）；
   只有 config.txt 里 motion = auto 时，html 才会带 .motion-auto，
   这时才尊重系统设置。site.js 里的滚动/补间动画同理。
   ============================================================ */

@media (prefers-reduced-motion: reduce) {
  html.motion-auto { scroll-behavior: auto; }
  html.motion-auto *,
  html.motion-auto *::before,
  html.motion-auto *::after {
    animation: none !important;
    transition-duration: .001ms !important;
  }
}

/* ============================================================
   响应式
   ============================================================ */

@media (max-width: 1020px) {
  :root { --side-w: 0px; }

  #progress { display: block; }
  .nav-toggle { display: block; }
  .shell { margin-left: 0; }

  .sidebar {
    width: 262px;
    transform: translateX(-102%);
    transition: transform .34s var(--ease-out);
    box-shadow: 0 0 60px rgba(0, 0, 0, .6);
  }
  body.nav-open .sidebar { transform: translateX(0); }

  main { padding: 0 14px; }
  .hero {
    grid-template-columns: 1fr;
    gap: 20px;
    min-height: 0;
    padding: 74px 20px 46px;
    text-align: left;
  }
  .hero::before { background-position: 62% 42%; transform: none; }
  .hero::after {
    background:
      linear-gradient(180deg,
        color-mix(in srgb, var(--bg) 78%, transparent) 0%,
        color-mix(in srgb, var(--bg) 52%, transparent) 42%,
        var(--bg) 100%);
  }
  .avatar { width: 104px; height: 104px; border-radius: 22px; }
  .board-hud { max-width: calc(100% - 28px); }
}

@media (max-width: 560px) {
  .block { padding: 24px 18px; border-radius: 16px; }
  .block h2 { font-size: 20px; }
  .hero h1 { font-size: clamp(34px, 11vw, 44px); }
  .dlg { padding: 11px 14px 12px 50px; }
  .dlg-who { left: 11px; top: 11px; width: 28px; height: 28px; font-size: 13px; }
  .cards { grid-template-columns: 1fr; }
  .board { min-height: 262px; }
  .board-hud { gap: 5px; }
  .spec { font-size: 11.5px; }
  .gate-links { grid-template-columns: repeat(2, minmax(0, 1fr)); }
}

/* ============================================================
   打印：去掉背景与浮层，直接当简历打
   ============================================================ */

@media print {
  .sidebar, .nav-toggle, .nav-scrim, #progress, #gate, #welcome,
  .fx-layer, .hero-scan, .cta, .board { display: none !important; }
  .shell { margin-left: 0; }
  body { background: #fff; color: #000; }
  .hero::before, .hero::after { display: none; }
  .hero h1, .hero .role, .hero .tagline, .eyebrow { color: #000; text-shadow: none; }
  .hero h1 .grad { -webkit-text-fill-color: #000; color: #000; }
  .block {
    background: none;
    border: 0;
    box-shadow: none;
    backdrop-filter: none;
    padding: 12px 0;
    break-inside: avoid;
  }
  .card, .dlg { background: none; }
  .reveal { opacity: 1 !important; transform: none !important; }
}
)CSS";

}  // namespace

std::string webStyleCss() {
    return std::string(kCssBase) + std::string(kCssBody) + std::string(kCssGate);
}

}  // namespace ssg
