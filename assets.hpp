// assets.hpp —— 站点"素材"（HTML / CSS / JS）全部以 C++ 字符串的形式提供。
//
// 这个项目里没有任何 .html / .css / .js 源文件：页面骨架、样式、动效脚本
// 都写在 .cpp 里（raw string literal），由生成器直接写进 dist/。
// 想看"页面长什么样"就打开对应的 .cpp，改完重新编译即可。
//
//   web_page.cpp    →  index.html   页面骨架（{{key}} 占位符由 C++ 替换）
//   web_style.cpp   →  style.css    样式表
//   web_script.cpp  →  site.js      站点行为（主题、导航、搜索、进场、视差…）
//   web_fx.cpp      →  fx.js        画面特效（像素火焰、光标粒子、芯片电流、像素 WELCOME）

#ifndef SSG_ASSETS_HPP
#define SSG_ASSETS_HPP

#include <string>

namespace ssg {

// 页面骨架，含 {{key}} 占位符
std::string webPageHtml();

// 样式表，同样支持 {{accent}} 之类的占位符（颜色由 C++ 注入）
std::string webStyleCss();

// 站点行为脚本：原样输出到 dist/site.js
std::string webScriptJs();

// 画面特效脚本：原样输出到 dist/fx.js
std::string webFxJs();

}  // namespace ssg

#endif  // SSG_ASSETS_HPP
