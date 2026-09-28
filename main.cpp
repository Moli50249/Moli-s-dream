// ssg —— 用 C++ 写的极简静态站点生成器。
//
// 读 content/ 下的配置与 Markdown，产出纯静态的 dist/（可直接推到 GitHub Pages）。
// 页面骨架、样式、脚本全部来自 src/*.cpp 里的字符串（见 assets.hpp），
// 所以这个项目里没有任何 .html / .css / .js 源文件。
//
// 用法：
//   ssg [内容目录] [输出目录]
// 缺省值为 content/ 和 dist/。

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "assets.hpp"
#include "markdown.hpp"
#include "template.hpp"

namespace fs = std::filesystem;
using ssg::Context;
using ssg::inlineMd;
using ssg::mdToHtml;
using ssg::render;
using ssg::webFxJs;
using ssg::webPageHtml;
using ssg::webScriptJs;
using ssg::webStyleCss;

namespace {

// ---------- 基础工具 ----------

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string line;
    while (std::getline(in, line)) out.push_back(trim(line));
    return out;
}

std::vector<std::string> splitBy(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
        if (c == sep) { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(trim(cur));
    return out;
}

// HTML 转义（用于属性值与纯文本）
std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '&':  o += "&amp;";  break;
            case '<':  o += "&lt;";   break;
            case '>':  o += "&gt;";   break;
            case '"':  o += "&quot;"; break;
            case '\'': o += "&#39;";  break;
            default:   o += c;        break;
        }
    }
    return o;
}

// JSON 字符串转义；顺手把 < 写成 \u003c，避免内容里出现 </script> 把页面截断
std::string jsonEsc(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (const unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            case '<':  o += "\\u003c"; break;
            case '>':  o += "\\u003e"; break;
            case '&':  o += "\\u0026"; break;
            default:
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    o += "\\u00";
                    o += hex[(c >> 4) & 0xF];
                    o += hex[c & 0xF];
                } else {
                    o += static_cast<char>(c);
                }
                break;
        }
    }
    return o;
}

// 取 UTF-8 字符串的第一个字符（中文一般占 3 字节）
std::string firstUtf8Char(const std::string& s) {
    if (s.empty()) return "?";
    const unsigned char c = static_cast<unsigned char>(s[0]);
    size_t n = 1;
    if (c >= 0xF0) n = 4;
    else if (c >= 0xE0) n = 3;
    else if (c >= 0xC0) n = 2;
    if (n > s.size()) n = s.size();
    return s.substr(0, n);
}

// 从 i 开始的那个 UTF-8 字符占几个字节（全角冒号是 3 字节，不能只 +1）
size_t utf8CharLenAt(const std::string& s, size_t i) {
    if (i >= s.size()) return 0;
    const unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = 1;
    if (c >= 0xF0) n = 4;
    else if (c >= 0xE0) n = 3;
    else if (c >= 0xC0) n = 2;
    return (i + n > s.size()) ? (s.size() - i) : n;
}

// 按 UTF-8 字符截断（不会把汉字切成半个）
std::string utf8Cut(const std::string& s, size_t chars, const std::string& tail = "…") {
    size_t i = 0, n = 0;
    while (i < s.size() && n < chars) { i += utf8CharLenAt(s, i); ++n; }
    return i >= s.size() ? s : s.substr(0, i) + tail;
}

// 把 Markdown 行内标记剥掉，得到可以放进属性/搜索结果里的纯文本
std::string plainMd(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size();) {
        if (s.compare(i, 2, "**") == 0) { i += 2; continue; }
        if (s[i] == '*' || s[i] == '`') { ++i; continue; }
        if (s[i] == '[') {
            const size_t cb = s.find(']', i + 1);
            if (cb != std::string::npos && cb + 1 < s.size() && s[cb + 1] == '(') {
                const size_t pe = s.find(')', cb + 2);
                if (pe != std::string::npos) {
                    o += s.substr(i + 1, cb - i - 1);
                    i = pe + 1;
                    continue;
                }
            }
        }
        o += s[i++];
    }
    return trim(o);
}

bool readTextFile(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

void writeBinaryFile(const fs::path& p, const std::vector<char>& data) {
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cerr << "[ssg] 无法写入: " << p.string() << "\n";
        return;
    }
    if (!data.empty()) f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void writeTextFile(const fs::path& p, const std::string& content) {
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cerr << "[ssg] 无法写入: " << p.string() << "\n";
        return;
    }
    f << content;
}

// ---------- 配置 ----------

Context defaultConfig() {
    return Context{
        {"name",      "张三"},
        {"title",     "一句话职位"},
        {"tagline",   "（在 content/config.txt 里写一句你的 tagline）"},
        {"location",  "城市"},
        {"email",     "you@example.com"},
        {"github",    "https://github.com/your-id"},
        {"accent",    "#5b8cff"},
        {"accent2",   "#8b5cf6"},
        {"accent3",   "#22c1a4"},
        {"domain",    ""},
        {"age_gate",  "on"},
        {"gate_btn",  "我保证我已满18岁！确定进入！"},
        {"gate_note", ""},
        {"gate_accent", "#ff7a18"},
        {"chip_label",  "CPP-SSG"},
        {"chip_sub",    "PERSONAL SITE"},
        // auto = 跟随系统的"减少动态效果"；always = 无论如何都播
        {"welcome",   "always"},
        {"motion",    "always"},
        {"footer",    "这个页面由 C++ 写的静态站点生成器构建，托管在 GitHub Pages。"},
    };
}

Context loadConfig(const fs::path& p) {
    Context ctx = defaultConfig();
    std::string raw;
    if (!readTextFile(p, raw)) {
        std::cerr << "[ssg] 警告: 未找到 " << p.string() << "，使用默认配置。\n";
        return ctx;
    }
    for (const std::string& line : splitLines(raw)) {
        if (line.empty() || startsWith(line, "#")) continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        // 留空表示"用默认值"，避免页面上出现空洞
        if (!key.empty() && !val.empty()) ctx[key] = val;
    }
    return ctx;
}

// ---------- 搜索索引 ----------

struct Hit {
    std::string label;    // 关键词（同时写进 data-k，查找时用来定位）
    std::string kind;     // 类型：区块 / 关于 / 技能 / 项目 / 经历 / 规格
    std::string section;  // 所属区块 id
};

// ---------- 各区块构建 ----------

// 技能：## 分组名（渲染成一个大框）/ - 名称 | 百分比（大框里的一个小框）
// groups：顺便把分组名收集出来，门上的九宫格可以按名字直接跳到大框
std::string buildSkills(const std::string& md, std::vector<Hit>& hits,
                        std::vector<std::string>& groups) {
    std::string out;
    bool open = false;
    std::string group;
    std::string head;        // 攒着大框的开头，等条目数统计完再一起吐出去
    std::string body;        // 大框里的小框
    size_t count = 0;

    auto openGroup = [&](const std::string& g) {
        group = g.empty() ? "技能" : g;
        head = "<div class=\"skill-group\" id=\"g-" + esc(group) + "\" data-k=\"" + esc(group) + "\">\n"
               "  <div class=\"skill-group-head\">\n"
               "    <h3>" + esc(group) + "</h3>\n"
               "    <span class=\"skill-group-n\">";
        body.clear();
        count = 0;
        open = true;
        groups.push_back(group);
        // 分组本身也进搜索索引：搜"游戏"能直接跳到这一框
        hits.push_back({group, "技能分组", "skills"});
    };

    auto closeGroup = [&]() {
        if (!open) return;
        out += head + std::to_string(count) + " 项</span>\n  </div>\n";
        out += "  <div class=\"skill-grid\">\n" + body + "  </div>\n";
        out += "</div>\n";
        open = false;
        head.clear();
        body.clear();
        count = 0;
    };

    for (const std::string& line : splitLines(md)) {
        if (line.empty()) continue;

        // 先认 "## 分组"：它也是 # 开头，不能先被"注释行"那条规则吃掉
        if (startsWith(line, "## ")) {
            closeGroup();
            openGroup(trim(line.substr(3)));
            continue;
        }
        // 其余 # 开头的是说明/注释
        if (startsWith(line, "#")) continue;

        if (!startsWith(line, "- ")) continue;

        const std::string item = trim(line.substr(2));
        const size_t bar = item.find('|');
        std::string name = item;
        int pct = 0;
        if (bar != std::string::npos) {
            name = trim(item.substr(0, bar));
            try {
                pct = std::stoi(trim(item.substr(bar + 1)));
            } catch (...) {
                pct = 0;
            }
        }
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        if (name.empty()) continue;

        if (!open) openGroup("技能");

        body += "    <div class=\"skill\" data-k=\"" + esc(name) + "\">\n";
        body += "      <div class=\"skill-head\"><span class=\"skill-name\">" + esc(name) +
                "</span><span class=\"skill-pct\">" + std::to_string(pct) + "%</span></div>\n";
        // 内联写入最终宽度：即使浏览器没跑 JS，进度条也是对的
        body += "      <div class=\"skill-track\"><div class=\"skill-fill\" data-pct=\"" +
                std::to_string(pct) + "\" style=\"width:" + std::to_string(pct) +
                "%\"></div></div>\n";
        body += "    </div>\n";

        ++count;
        hits.push_back({name, "技能 · " + group, "skills"});
    }
    closeGroup();
    return out;
}

// 关于我：连续以「名字：」开头的行会被渲染成对话，其余照常走 Markdown。
// 末尾形如「- 名称 | 百分比」的行会收成"自评条"放在最下面（比如 小资历 | 100）。
std::string buildAbout(const std::string& md, std::vector<Hit>& hits) {
    std::string out;
    std::string plain;
    std::string bars;      // 攒起来的自评条，最后统一放到区块底部
    bool openDlg = false;

    auto flushPlain = [&]() {
        if (!plain.empty()) {
            out += mdToHtml(plain);
            plain.clear();
        }
    };
    auto flushDlg = [&]() {
        if (openDlg) {
            out += "</div>\n";
            openDlg = false;
        }
    };

    for (const std::string& line : splitLines(md)) {
        // 空行是中性分隔：不打断已经开始的对话组
        if (line.empty()) {
            if (!openDlg) flushPlain();
            continue;
        }

        // 自评条：- 名称 | 百分比
        if (startsWith(line, "- ")) {
            const std::string item = trim(line.substr(2));
            const size_t bar = item.find('|');
            if (bar != std::string::npos) {
                const std::string nm = trim(item.substr(0, bar));
                int pct = -1;
                try {
                    pct = std::stoi(trim(item.substr(bar + 1)));
                } catch (...) {
                    pct = -1;
                }
                if (!nm.empty() && pct >= 0) {
                    if (pct > 100) pct = 100;
                    flushDlg();
                    flushPlain();
                    bars += "  <div class=\"skill\" data-k=\"" + esc(nm) + "\">\n";
                    bars += "    <div class=\"skill-head\"><span class=\"skill-name\">" + esc(nm) +
                            "</span><span class=\"skill-pct\">" + std::to_string(pct) +
                            "%</span></div>\n";
                    bars += "    <div class=\"skill-track\"><div class=\"skill-fill\" data-pct=\"" +
                            std::to_string(pct) + "\" style=\"width:" + std::to_string(pct) +
                            "%\"></div></div>\n";
                    bars += "  </div>\n";
                    hits.push_back({nm, "关于 · 自评", "about"});
                    continue;
                }
            }
        }

        // 对话行：形如 "莫璃： 内容"（冒号全角半角都认）
        const size_t colon = line.find_first_of("：:");
        const bool isDialogue =
            !startsWith(line, "#") && colon != std::string::npos &&
            colon > 0 && colon <= 12;

        if (isDialogue) {
            flushPlain();
            if (!openDlg) {
                out += "<div class=\"dialogue\">\n";
                openDlg = true;
            }
            const std::string who = trim(line.substr(0, colon));
            // 冒号可能是全角（3 字节），必须整体跳过
            const std::string raw = trim(line.substr(colon + utf8CharLenAt(line, colon)));
            const std::string text = plainMd(raw);
            const std::string first = firstUtf8Char(who);
            if (text.empty()) continue;

            // 关键词带上说话人：搜"莫璃"能把这几句自述都找出来
            const std::string label = who + "：" + text;

            out += "  <div class=\"dlg\" data-k=\"" + esc(label) + "\">\n";
            out += "    <span class=\"dlg-who\" title=\"" + esc(who) + "\">" +
                   esc(first) + "</span>\n";
            out += "    <p class=\"dlg-text\">" + inlineMd(first + "：" + raw) + "</p>\n";
            out += "  </div>\n";

            hits.push_back({label, "关于 · 自述", "about"});
            continue;
        }

        flushDlg();
        plain += line + "\n";
    }

    flushDlg();
    flushPlain();
    if (!bars.empty()) out += "<div class=\"about-bars\">\n" + bars + "</div>\n";
    return out;
}

// 项目：每个 ## 一节，正文走 Markdown
std::string buildProjects(const std::string& md, std::vector<Hit>& hits) {
    std::string out;

    std::vector<std::string> blocks;
    std::string cur;
    for (const std::string& line : splitLines(md)) {
        if (startsWith(line, "## ") && !cur.empty()) {
            blocks.push_back(cur);
            cur.clear();
        }
        if (startsWith(line, "# ") || startsWith(line, "#")) {
            if (startsWith(line, "# ")) continue;
        }
        cur += line + "\n";
    }
    if (!cur.empty()) blocks.push_back(cur);

    for (const std::string& block : blocks) {
        const std::string b = trim(block);
        if (b.empty() || !startsWith(b, "## ")) continue;

        const size_t nl = b.find('\n');
        std::string title = trim(b.substr(3, nl == std::string::npos ? std::string::npos : nl - 3));
        std::string body;
        if (nl != std::string::npos) body = trim(b.substr(nl + 1));
        if (title.empty()) continue;

        out += "<article class=\"card\" data-k=\"" + esc(title) + "\">\n";
        out += "  <h3>" + esc(title) + "</h3>\n";
        if (!body.empty()) out += "  " + mdToHtml(body) + "\n";
        out += "</article>\n";

        hits.push_back({title, "项目", "projects"});
    }
    return out;
}

// 时间线：- 时间 | 标题 | 描述
std::string buildTimeline(const std::string& md, std::vector<Hit>& hits) {
    std::string out;
    bool open = false;

    for (const std::string& line : splitLines(md)) {
        if (line.empty() || startsWith(line, "#")) continue;
        if (!startsWith(line, "- ")) continue;

        const std::vector<std::string> parts = splitBy(trim(line.substr(2)), '|');
        const std::string when = parts.size() > 0 ? parts[0] : "";
        const std::string what = parts.size() > 1 ? parts[1] : "";
        const std::string desc = parts.size() > 2 ? parts[2] : "";
        if (what.empty() && when.empty()) continue;

        if (!open) { out += "<ol class=\"timeline\">\n"; open = true; }

        out += "  <li data-k=\"" + esc(what.empty() ? when : what) + "\">\n";
        out += "    <div class=\"t-when\">" + esc(when) + "</div>\n";
        out += "    <div class=\"t-body\">\n";
        out += "      <h4>" + esc(what) + "</h4>\n";
        if (!desc.empty()) out += "      <p>" + esc(desc) + "</p>\n";
        out += "    </div>\n";
        out += "  </li>\n";

        if (!what.empty()) hits.push_back({what, "经历" + (when.empty() ? "" : " · " + when), "timeline"});
    }
    if (open) out += "</ol>\n";
    return out;
}

// ---------- 侧栏导航 ----------

struct Section {
    std::string id;
    std::string num;
    std::string title;
};

std::vector<Section> sectionList() {
    return {
        {"about",    "01", "关于我"},
        {"skills",   "02", "技能"},
        {"projects", "03", "项目"},
        {"timeline", "04", "经历"},
        {"core",     "05", "核心面板"},
    };
}

std::string buildNav(const std::vector<Section>& secs, const std::vector<Hit>& hits) {
    std::ostringstream s;
    for (const Section& sec : secs) {
        size_t n = 0;
        for (const Hit& h : hits) if (h.section == sec.id) ++n;
        s << "    <a class=\"side-link\" href=\"#" << sec.id << "\" data-target=\"" << sec.id << "\">\n";
        s << "      <span class=\"n\">" << sec.num << "</span>\n";
        s << "      <span class=\"t\">" << esc(sec.title) << "</span>\n";
        if (n) s << "      <span class=\"c\">" << n << "</span>\n";
        s << "    </a>\n";
    }
    return s.str();
}

// 搜索索引：区块名 + 每条内容，前端拿它做"快速查找"
std::string buildIndex(const std::vector<Section>& secs, std::vector<Hit> hits) {
    auto titleOf = [&secs](const std::string& id) -> std::string {
        for (const Section& s : secs) if (s.id == id) return s.title;
        return id;
    };

    std::vector<Hit> all;
    for (const Section& sec : secs) all.push_back({sec.title, "区块", sec.id});
    for (const Hit& h : hits) all.push_back(h);

    std::ostringstream s;
    s << "[";
    for (size_t i = 0; i < all.size(); ++i) {
        if (i) s << ",";
        s << "{\"label\":\"" << jsonEsc(all[i].label) << "\","
          << "\"kind\":\"" << jsonEsc(all[i].kind) << "\","
          << "\"t\":\"" << jsonEsc(titleOf(all[i].section)) << "\","
          << "\"s\":\"" << jsonEsc(all[i].section) << "\"}";
    }
    s << "]";
    return s.str();
}

// ---------- 头像 / favicon：C++ 直接画 SVG ----------

std::string buildAvatarSvg(const Context& ctx) {
    const std::string name = ctx.count("name") ? ctx.at("name") : "?";
    const std::string a1 = ctx.count("accent") ? ctx.at("accent") : "#5b8cff";
    const std::string a2 = ctx.count("accent2") ? ctx.at("accent2") : "#8b5cf6";
    const std::string ch = firstUtf8Char(name);

    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 200 200\" "
         "width=\"200\" height=\"200\" role=\"img\">\n";
    s << "  <defs>\n";
    s << "    <linearGradient id=\"ag\" x1=\"0\" y1=\"1\" x2=\"1\" y2=\"0\">\n";
    s << "      <stop offset=\"0\" stop-color=\"" << a1 << "\"/>\n";
    s << "      <stop offset=\"1\" stop-color=\"" << a2 << "\"/>\n";
    s << "    </linearGradient>\n";
    s << "  </defs>\n";
    s << "  <rect width=\"200\" height=\"200\" rx=\"100\" fill=\"url(#ag)\"/>\n";
    s << "  <text x=\"100\" y=\"104\" text-anchor=\"middle\" "
         "dominant-baseline=\"central\" font-family=\"system-ui, -apple-system, "
         "Segoe UI, Roboto, Helvetica, Arial, sans-serif\" font-size=\"92\" "
         "font-weight=\"600\" fill=\"#ffffff\">"
      << esc(ch) << "</text>\n";
    s << "</svg>\n";
    return s.str();
}

// 用编译日期推出年份，避免依赖运行环境的时间
std::string compileYear() {
    const std::string d = __DATE__;  // 形如 "Sep 27 2026"
    if (d.size() < 4) return "2026";
    return d.substr(d.size() - 4);
}

// 把 content/ 下的一张图片按原名复制进输出目录，返回输出用的文件名
std::string copyImage(const fs::path& contentDir, const fs::path& outDir,
                      const std::string& base,
                      std::initializer_list<const char*> exts) {
    for (const char* ext : exts) {
        const std::string name = base + std::string(ext);
        const fs::path src = contentDir / name;
        if (!fs::exists(src)) continue;

        std::ifstream in(src, std::ios::binary | std::ios::ate);
        if (!in) continue;
        const std::streamsize n = in.tellg();
        in.seekg(0, std::ios::beg);
        std::vector<char> data(static_cast<size_t>(n > 0 ? n : 0));
        if (n > 0) in.read(data.data(), n);

        writeBinaryFile(outDir / name, data);
        return name;
    }
    return "";
}

// ---------- 颜色工具：主题色压在什么底色上、浅色主题怎么压暗，都由 C++ 算 ----------

std::string cssColor(const std::string& v, const std::string& fallback) {
    const std::string s = trim(v);
    if (s.size() == 4 && s[0] == '#') {
        for (size_t i = 1; i < 4; ++i) {
            if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return fallback;
        }
        return s;
    }
    if (s.size() == 7 && s[0] == '#') {
        for (size_t i = 1; i < 7; ++i) {
            if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return fallback;
        }
        return s;
    }
    return fallback;
}

struct Rgb { int r; int g; int b; };

bool parseHexColor(const std::string& hex, Rgb& out) {
    const std::string h = trim(hex);
    auto v = [&h](size_t i) -> int {
        if (i >= h.size()) return 0;
        const char c = h[i];
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    if (h.size() == 7 && h[0] == '#') {
        out = Rgb{v(1) * 16 + v(2), v(3) * 16 + v(4), v(5) * 16 + v(6)};
        return true;
    }
    if (h.size() == 4 && h[0] == '#') {
        out = Rgb{v(1) * 17, v(2) * 17, v(3) * 17};
        return true;
    }
    return false;
}

std::string toHex(const Rgb& c) {
    auto byte = [](int v) -> std::string {
        const int x = v < 0 ? 0 : (v > 255 ? 255 : v);
        const char* d = "0123456789abcdef";
        std::string s;
        s += d[(x >> 4) & 0xF];
        s += d[x & 0xF];
        return s;
    };
    return "#" + byte(c.r) + byte(c.g) + byte(c.b);
}

double luminance(const Rgb& c) {
    return (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) / 255.0;
}

// 按底色亮度决定压在上面的文字用深色还是浅色
std::string onColorFor(const std::string& hex) {
    Rgb c;
    if (!parseHexColor(hex, c)) return "#ffffff";
    return luminance(c) > 0.58 ? "#15181f" : "#ffffff";
}

// 浅色主题下把强调色压暗：浅色强调色在白底上等于没有
std::string darkenFor(const std::string& hex, double factor) {
    Rgb c;
    if (!parseHexColor(hex, c)) return hex;
    auto d = [factor](int v) { return static_cast<int>(v * factor); };
    return toHex(Rgb{d(c.r), d(c.g), d(c.b)});
}

// 粗判是否为可用域名：有点号、无空白、不含协议或路径
bool looksLikeDomain(const std::string& s) {
    if (s.empty()) return false;
    for (const char c : s) {
        if (c == ' ' || c == '\t' || c == '/' || c == ':') return false;
    }
    if (s.find('.') == std::string::npos) return false;
    if (s.front() == '.' || s.back() == '.') return false;
    return true;
}

// ---------- 像素吉祥物：字符网格直接写在 C++ 里 ----------
// 照着方格纸上的那张像素骷髅（黑线 + 一点青蓝）逐格采样点出来的，23×19 格，
// 已经裁掉了四周空白的格子。
// 字符表：'#' 黑线  '.' 纸面  'C' 深青  'c' 浅青
//        （通用色：'w' 白 's' 皮肤 'h' 头发 'p' 眼睛 'm' 嘴 'k' 深色 'b' 蓝色）
const char* const kMascot[] = {
    ".......#########.......",
    ".....##.........##.....",
    "....#.............#....",
    "....#.............#....",
    "...#...............#...",
    "...#..###.....###..#...",
    "...#..###.....###..#...",
    "...#..###..#..###..#...",
    "....#.....###.....#....",
    "...##.#.........#.##...",
    "...#..###########..#...",
    "...#...#.#.#.#.#...#...",
    "....##..#######..##....",
    "...#####.......#####...",
    "..#C#cc#########cc#C#..",
    ".#CCC#cc#..#..#cc#CCC#.",
    ".#CCCC#####.#####CCCC#.",
    "#CC#CCCC#..#..#CCCC#CC#",
    "#CCC#CCC#.....#CCC#CCC#",
};

// 黑线外面的白色是"纸"，不是图案：从画面四边往里漫水（四连通），
// 能走到的白格 = 背景，输出成透明；走不到的（被黑线围住的颅内、牙齿、
// 下巴这些）才是图案的一部分，留成白色 'w'。
std::vector<std::string> cutOutsidePaper() {
    const size_t rows = sizeof(kMascot) / sizeof(kMascot[0]);
    size_t cols = 0;
    for (size_t i = 0; i < rows; ++i) cols = std::max(cols, std::string(kMascot[i]).size());

    std::vector<std::string> g;
    g.reserve(rows);
    for (size_t i = 0; i < rows; ++i) {
        std::string line(kMascot[i]);
        line.resize(cols, '.');
        g.push_back(line);
    }

    std::vector<char> outside(rows * cols, 0);
    std::vector<std::pair<size_t, size_t>> stack;

    auto visit = [&](long long y, long long x) {
        if (y < 0 || x < 0 || y >= static_cast<long long>(rows) || x >= static_cast<long long>(cols)) return;
        const size_t idx = static_cast<size_t>(y) * cols + static_cast<size_t>(x);
        if (outside[idx]) return;
        if (g[y][x] != '.') return;      // 只漫白格，黑线/青色挡住
        outside[idx] = 1;
        stack.push_back({static_cast<size_t>(y), static_cast<size_t>(x)});
    };

    for (size_t x = 0; x < cols; ++x) { visit(0, static_cast<long long>(x)); visit(static_cast<long long>(rows) - 1, static_cast<long long>(x)); }
    for (size_t y = 0; y < rows; ++y) { visit(static_cast<long long>(y), 0); visit(static_cast<long long>(y), static_cast<long long>(cols) - 1); }

    while (!stack.empty()) {
        const auto [y, x] = stack.back();
        stack.pop_back();
        const long long sy = static_cast<long long>(y);
        const long long sx = static_cast<long long>(x);
        visit(sy - 1, sx); visit(sy + 1, sx); visit(sy, sx - 1); visit(sy, sx + 1);
    }

    for (size_t y = 0; y < rows; ++y) {
        for (size_t x = 0; x < cols; ++x) {
            if (g[y][x] == '.' && !outside[y * cols + x]) g[y][x] = 'w';   // 图案内部的白
        }
    }
    return g;
}

std::string buildSpriteJson() {
    const std::vector<std::string> art = cutOutsidePaper();
    std::ostringstream s;
    s << "{\"scale\":3,\"rows\":[";
    for (size_t i = 0; i < art.size(); ++i) {
        if (i) s << ",";
        s << "\"" << art[i] << "\"";
    }
    s << "]}";
    return s.str();
}

// ---------- 进场门 + WELCOME 层 ----------

// 技能分组名列表（用 '|' 拼起来的）里有没有这一组
bool hasGroup(const std::string& joined, const std::string& name) {
    if (trim(joined).empty() || name.empty()) return false;
    for (const std::string& g : splitBy(joined, '|')) {
        if (g == name) return true;
    }
    return false;
}

// 解析九宫格入口配置："标签:#锚点, 标签:@github, 标签:https://..."
std::vector<std::pair<std::string, std::string>> parseGateLinks(const std::string& raw) {
    std::vector<std::pair<std::string, std::string>> out;
    if (trim(raw).empty()) return out;
    for (const std::string& item : splitBy(raw, ',')) {
        if (item.empty()) continue;
        const size_t c = item.find(':');
        if (c == std::string::npos) continue;
        const std::string label = trim(item.substr(0, c));
        const std::string href = trim(item.substr(c + 1));
        if (label.empty() || href.empty()) continue;
        out.push_back({label, href});
    }
    return out;
}

struct Digest {
    std::vector<std::string> about;    // 关于我：几句话
    std::vector<std::string> skills;   // 技能名
    std::vector<std::string> projects; // 项目名
    std::vector<std::string> timeline; // 经历
};

Digest buildDigest(const fs::path& contentDir) {
    Digest d;
    std::string raw;

    if (readTextFile(contentDir / "about.md", raw)) {
        for (const std::string& line : splitLines(raw)) {
            if (line.empty() || startsWith(line, "#") || line == "---") continue;
            const size_t colon = line.find_first_of("：:");
            std::string text = line;
            if (colon != std::string::npos && colon > 0 && colon <= 12) {
                text = trim(line.substr(colon + utf8CharLenAt(line, colon)));
            }
            text = plainMd(text);
            if (!text.empty() && d.about.size() < 3) d.about.push_back(utf8Cut(text, 44));
        }
    }
    if (readTextFile(contentDir / "skills.md", raw)) {
        for (const std::string& line : splitLines(raw)) {
            if (!startsWith(line, "- ")) continue;
            const std::string item = trim(line.substr(2));
            const size_t bar = item.find('|');
            const std::string name = trim(bar == std::string::npos ? item : item.substr(0, bar));
            if (!name.empty() && name.size() < 24) d.skills.push_back(name);
            if (d.skills.size() >= 8) break;
        }
    }
    if (readTextFile(contentDir / "projects.md", raw)) {
        for (const std::string& line : splitLines(raw)) {
            if (!startsWith(line, "## ")) continue;
            const std::string t = trim(line.substr(3));
            if (!t.empty()) d.projects.push_back(t);
            if (d.projects.size() >= 4) break;
        }
    }
    if (readTextFile(contentDir / "timeline.md", raw)) {
        for (const std::string& line : splitLines(raw)) {
            if (!startsWith(line, "- ")) continue;
            const std::vector<std::string> parts = splitBy(trim(line.substr(2)), '|');
            const std::string when = parts.size() > 0 ? parts[0] : "";
            const std::string what = parts.size() > 1 ? parts[1] : "";
            if (!what.empty()) d.timeline.push_back(when + " · " + utf8Cut(what, 30));
            if (d.timeline.size() >= 4) break;
        }
    }
    return d;
}

// 进场门 + WELCOME 赛博加载层。
// 这层只是覆盖在正文之上的浮层，正文始终完整存在于 DOM 里，
// 所以搜索引擎和链接分享的预览照样能抓到内容；JS 不可用时门根本不显示。
std::string buildGate(const Context& ctx, const Digest& digest) {
    const std::string flag = ctx.count("age_gate") ? trim(ctx.at("age_gate")) : "on";
    const bool on = (flag == "on" || flag == "true" || flag == "1" || flag == "yes");
    if (!on) return "";

    auto get = [&](const std::string& k, const std::string& fb) {
        return ctx.count(k) ? ctx.at(k) : fb;
    };
    const std::string name = get("name", "");
    const std::string title = get("title", "");
    const std::string tagline = get("tagline", "");
    const std::string location = get("location", "");
    const std::string email = get("email", "");
    const std::string github = get("github", "");
    const std::string btn = get("gate_btn", "确定进入");
    const std::string note = get("gate_note", "");
    const std::string avatar = get("avatar", "avatar.svg");
    const std::string wallpaper = get("wallpaper", "");
    const std::string cover = wallpaper.empty() ? avatar : wallpaper;

    std::vector<std::pair<std::string, std::string>> links =
        parseGateLinks(get("gate_links", ""));
    if (links.empty()) {
        // 默认九宫格：5 个站内区块 + 游戏 + GitHub + 邮箱 + 回到首屏 = 3×3
        // 技能里有"游戏"分组就跳到那个大框，没有就退回技能区
        const std::string gameTarget =
            hasGroup(get("skill_groups", ""), "游戏") ? "#g-游戏" : "#skills";
        links = {
            {"关于我", "#about"},     {"技能", "#skills"},    {"项目", "#projects"},
            {"经历", "#timeline"},    {"核心面板", "#core"},  {"游戏", gameTarget},
            {"GitHub", "@github"},    {"邮箱", "@email"},     {"回到首屏", "#top"},
        };
    }

    auto resolve = [&](const std::string& h) -> std::string {
        if (h == "@github") return github.empty() ? "#" : github;
        if (h == "@email") return "mailto:" + email;
        return h;
    };

    std::ostringstream s;
    s << "<div id=\"gate\" role=\"dialog\" aria-label=\"进入确认\">\n";
    s << "  <div class=\"gate-stage\">\n";
    s << "    <div class=\"gate-slides\">\n";

    // 第 1 屏：封面（壁纸）+ 欢迎牌 + 像素横条
    s << "      <figure class=\"gate-slide is-cover\">\n";
    s << "        <div class=\"gate-poster\">\n";
    s << "          <img src=\"" << esc(cover) << "\" alt=\"" << esc(name) << "\">\n";
    s << "          <div class=\"gate-plaque\"><i>欢迎来到</i><b>" << esc(name) << "</b></div>\n";
    s << "        </div>\n";
    s << "        <div class=\"gate-strip\">\n";
    s << "          <canvas width=\"48\" height=\"40\" aria-hidden=\"true\"></canvas>\n";
    s << "          <b>" << esc(name) << " 发布页</b>\n";
    s << "          <span>RELEASE</span>\n";
    s << "        </div>\n";
    s << "      </figure>\n";

    // 第 2 屏：头像 + 一句话
    s << "      <figure class=\"gate-slide\">\n";
    s << "        <div class=\"gate-card\">\n";
    s << "          <img class=\"gate-face\" src=\"" << esc(avatar) << "\" alt=\"" << esc(name) << "\">\n";
    s << "          <h3>" << esc(name) << "</h3>\n";
    s << "          <p>" << esc(title) << (tagline.empty() ? "" : "<br>" + esc(tagline)) << "</p>\n";
    if (!digest.about.empty()) {
        s << "          <div class=\"gate-mini\">\n";
        for (size_t i = 0; i < digest.about.size() && i < 2; ++i) {
            s << "            <div><span>" << (i == 0 ? "自述" : "&nbsp;") << "</span>"
              << esc(digest.about[i]) << "</div>\n";
        }
        s << "          </div>\n";
    }
    s << "        </div>\n";
    s << "      </figure>\n";

    // 第 3 屏：站内速览
    s << "      <figure class=\"gate-slide\">\n";
    s << "        <div class=\"gate-card\">\n";
    s << "          <h3>站内速览</h3>\n";
    s << "          <div class=\"gate-mini\">\n";
    if (!digest.skills.empty()) {
        std::string joined;
        for (size_t i = 0; i < digest.skills.size(); ++i) {
            if (i) joined += " · ";
            joined += digest.skills[i];
        }
        s << "            <div><span>技能</span>" << esc(utf8Cut(joined, 60)) << "</div>\n";
    }
    if (!digest.projects.empty()) {
        std::string joined;
        for (size_t i = 0; i < digest.projects.size(); ++i) {
            if (i) joined += " · ";
            joined += digest.projects[i];
        }
        s << "            <div><span>项目</span>" << esc(utf8Cut(joined, 40)) << "</div>\n";
    }
    if (!digest.timeline.empty()) {
        std::string joined;
        for (size_t i = 0; i < digest.timeline.size() && i < 3; ++i) {
            if (i) joined += " ／ ";
            joined += digest.timeline[i];
        }
        s << "            <div><span>经历</span>" << esc(utf8Cut(joined, 60)) << "</div>\n";
    }
    s << "          </div>\n";
    s << "        </div>\n";
    s << "      </figure>\n";

    // 第 4 屏：联系方式
    s << "      <figure class=\"gate-slide\">\n";
    s << "        <div class=\"gate-card\">\n";
    s << "          <h3>联系方式</h3>\n";
    s << "          <div class=\"gate-contact\">\n";
    if (!location.empty()) s << "            <span>位置：<b>" << esc(location) << "</b></span>\n";
    if (!email.empty())
        s << "            <span>邮箱：<a href=\"mailto:" << esc(email) << "\">" << esc(email) << "</a></span>\n";
    if (!github.empty())
        s << "            <span>GitHub：<a href=\"" << esc(github) << "\" target=\"_blank\" rel=\"noopener\">"
          << esc(github) << "</a></span>\n";
    if (!note.empty()) s << "            <span>" << esc(note) << "</span>\n";
    s << "          </div>\n";
    s << "        </div>\n";
    s << "      </figure>\n";

    s << "    </div>\n";

    // 圆点：数量跟屏数一致
    s << "    <div class=\"gate-dots\" role=\"tablist\" aria-label=\"封面切换\">\n";
    for (int i = 0; i < 4; ++i) {
        s << "      <button type=\"button\"" << (i == 0 ? " class=\"on\"" : "")
          << " aria-label=\"第 " << (i + 1) << " 屏\"></button>\n";
    }
    s << "    </div>\n";

    s << "    <button id=\"gate-btn\" type=\"button\">\n";
    s << "      <span class=\"gate-btn-fill\"></span>\n";
    s << "      <span class=\"gate-btn-label\">" << esc(btn) << "</span>\n";
    s << "    </button>\n";
    if (!note.empty()) s << "    <p class=\"gate-note\">" << esc(note) << "</p>\n";

    s << "    <nav class=\"gate-links\">\n";
    for (const auto& item : links) {
        const std::string href = resolve(item.second);
        const bool ext = href.rfind("http", 0) == 0;
        s << "      <a href=\"" << esc(href) << "\""
          << (ext ? " target=\"_blank\" rel=\"noopener\"" : "") << ">"
          << esc(item.first) << "</a>\n";
    }
    s << "    </nav>\n";
    s << "  </div>\n";
    s << "</div>\n";

    // 像素吉祥物的"点阵"由 C++ 给出，fx.js 负责上色
    s << "<script type=\"application/json\" id=\"sprite-data\">" << buildSpriteJson() << "</script>\n";
    return s.str();
}

// WELCOME 赛博像素加载层。
// 和门分开生成：门可以在 config 里关掉，但"进门后的 WELCOME 动画"独立存在
// —— 关掉门时访问者一进来就能看到它，而不是什么都没有。
std::string buildWelcomeLayer(const Context& ctx) {
    const std::string mode = ctx.count("welcome") ? trim(ctx.at("welcome")) : "always";
    const bool off = (mode == "off" || mode == "none" || mode == "false" || mode == "0");
    if (off) return "";

    std::ostringstream s;
    s << "<div id=\"welcome\" aria-hidden=\"true\">\n";
    s << "  <canvas id=\"welcome-canvas\" width=\"480\" height=\"180\"></canvas>\n";
    s << "  <div class=\"w-scan\" aria-hidden=\"true\"></div>\n";
    s << "  <div class=\"w-hud\">\n";
    s << "    <div class=\"w-bar\"><i></i></div>\n";
    s << "    <div class=\"w-log\"></div>\n";
    s << "  </div>\n";
    s << "</div>\n";
    return s.str();
}

// ---------- 首屏数字、芯片栏规格 ----------

std::string buildStats(size_t skills, size_t projects, size_t timeline) {
    std::ostringstream s;
    auto li = [&](size_t n, const std::string& unit) {
        s << "<li><b>" << n << "</b>" << unit << "</li>";
    };
    li(skills, "项技能");
    li(projects, "个项目");
    li(timeline, "段经历");
    s << "<li><b>C++</b>写的生成器</li>";
    return s.str();
}

// 芯片栏右侧的规格表；同时作为搜索条目
std::string buildCoreSpecs(const Context& ctx, std::vector<Hit>& hits) {
    auto get = [&](const std::string& k, const std::string& fb) {
        return ctx.count(k) ? ctx.at(k) : fb;
    };
    struct Row { std::string key; std::string label; std::string value; std::string html; };
    std::vector<Row> rows;

    const std::string name = get("name", "");
    const std::string location = get("location", "");
    const std::string email = get("email", "");
    const std::string github = get("github", "");
    const std::string chipLabel = get("chip_label", "CPP-SSG");
    const std::string chipSub = get("chip_sub", "PERSONAL SITE");

    if (!name.empty()) rows.push_back({"NAME", name, name, esc(name)});
    if (!location.empty()) rows.push_back({"NODE", location, location, esc(location)});
    if (!email.empty())
        rows.push_back({"MAIL", email, email, "<a href=\"mailto:" + esc(email) + "\">" + esc(email) + "</a>"});
    if (!github.empty()) {
        std::string shortName = github;
        const size_t slash = github.find_last_of('/');
        if (slash != std::string::npos && slash + 1 < github.size()) shortName = github.substr(slash + 1);
        rows.push_back({"SOURCE", shortName, github,
                        "<a href=\"" + esc(github) + "\" target=\"_blank\" rel=\"noopener\">" + esc(shortName) + "</a>"});
    }
    rows.push_back({"CHIP", chipLabel, chipLabel, esc(chipLabel)});
    rows.push_back({"BUILD", chipSub, chipSub, esc(chipSub)});

    std::ostringstream s;
    for (const Row& r : rows) {
        s << "      <div class=\"spec\" data-k=\"" << esc(r.label) << "\"><i>" << esc(r.key)
          << "</i><b>" << r.html << "</b></div>\n";
        hits.push_back({r.label, "规格", "core"});
    }
    return s.str();
}

}  // namespace

int main(int argc, char** argv) {
    // 参数分两类：--开头的是开关，其余按顺序是 内容目录 / 输出目录
    std::vector<std::string> pos;
    bool noGate = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (startsWith(a, "--")) {
            if (a == "--no-gate" || a == "--nogate") noGate = true;
            else std::cerr << "[ssg] 警告: 不认识的参数 " << a << "（已忽略）\n";
        } else {
            pos.push_back(a);
        }
    }

    fs::path contentDir = pos.size() > 0 ? fs::path(pos[0]) : fs::path("content");
    fs::path outDir = pos.size() > 1 ? fs::path(pos[1]) : fs::path("dist");

    // 兼容老写法：ssg content template dist —— 模板目录已经不需要了
    if (pos.size() > 2) {
        std::cerr << "[ssg] 提示: 模板目录参数已废弃（页面现在写在 src/*.cpp 里），"
                     "已忽略 " << pos[1] << "，输出到 " << pos[2] << "\n";
        outDir = fs::path(pos[2]);
    }

    if (!fs::exists(contentDir)) {
        std::cerr << "[ssg] 错误: 内容目录不存在: " << contentDir.string() << "\n";
        return 1;
    }

    // 1. 读配置
    Context ctx = loadConfig(contentDir / "config.txt");
    if (noGate) {
        ctx["age_gate"] = "off";
        std::cout << "[ssg] --no-gate: 本次生成跳过进场门（预览用）\n";
    }
    ctx["accent"] = cssColor(ctx["accent"], "#5b8cff");
    ctx["accent2"] = cssColor(ctx["accent2"], "#8b5cf6");
    ctx["accent3"] = cssColor(ctx["accent3"], "#22c1a4");
    ctx["on_accent"] = onColorFor(ctx["accent"]);
    ctx["accent_light"] = darkenFor(ctx["accent"], 0.55);
    ctx["accent2_light"] = darkenFor(ctx["accent2"], 0.72);
    ctx["accent3_light"] = darkenFor(ctx["accent3"], 0.62);
    ctx["on_accent_light"] = onColorFor(ctx["accent_light"]);

    // 进场门按钮颜色（图 1 那种橙色），同样保证按钮上的字看得清
    ctx["gate_accent"] = cssColor(ctx["gate_accent"], "#ff7a18");
    ctx["gate_on_accent"] = onColorFor(ctx["gate_accent"]);

    // 2. 读各段内容，顺便攒出搜索索引
    auto load = [&](const std::string& file) -> std::string {
        std::string s;
        if (!readTextFile(contentDir / file, s)) {
            std::cerr << "[ssg] 提示: 缺少 content/" << file << "，该区块将留空。\n";
            return "";
        }
        return s;
    };

    std::vector<Hit> hits;
    std::vector<std::string> skillGroups;
    const std::string aboutMd = load("about.md");
    const std::string skillsMd = load("skills.md");
    const std::string projectsMd = load("projects.md");
    const std::string timelineMd = load("timeline.md");

    ctx["about"] = buildAbout(aboutMd, hits);
    ctx["skills"] = buildSkills(skillsMd, hits, skillGroups);
    ctx["projects"] = buildProjects(projectsMd, hits);
    ctx["timeline"] = buildTimeline(timelineMd, hits);
    if (!ctx.count("year")) ctx["year"] = compileYear();

    // 技能分组名给门上九宫格用（比如"游戏"那一格直接跳到大框）
    {
        std::string joined;
        for (size_t i = 0; i < skillGroups.size(); ++i) {
            if (i) joined += "|";
            joined += skillGroups[i];
        }
        ctx["skill_groups"] = joined;
    }

    // 统计一下条数，给首屏数字和导航用
    size_t nSkills = 0, nProjects = 0, nTimeline = 0;
    for (const Hit& h : hits) {
        if (h.section == "skills") ++nSkills;
        else if (h.section == "projects") ++nProjects;
        else if (h.section == "timeline") ++nTimeline;
    }
    ctx["stats"] = buildStats(nSkills, nProjects, nTimeline);
    ctx["core_specs"] = buildCoreSpecs(ctx, hits);

    const std::vector<Section> secs = sectionList();
    ctx["nav"] = buildNav(secs, hits);
    ctx["index"] = buildIndex(secs, hits);

    // 3. 头像 / 壁纸
    const std::string avatarRef =
        copyImage(contentDir, outDir, "avatar", {".png", ".jpg", ".jpeg", ".webp", ".gif"});
    ctx["avatar"] = avatarRef.empty() ? "avatar.svg" : avatarRef;
    ctx["avatar_shape"] = avatarRef.empty() ? "glyph" : "photo";
    if (!avatarRef.empty()) std::cout << "[ssg] 头像: content/" << avatarRef << "\n";

    const std::string wallpaperRef =
        copyImage(contentDir, outDir, "wallpaper", {".jpg", ".jpeg", ".png", ".webp"});
    ctx["wallpaper"] = wallpaperRef;
    ctx["wallpaper_css"] = wallpaperRef.empty() ? "none" : "url(\"" + wallpaperRef + "\")";
    std::cout << "[ssg] 壁纸: "
              << (wallpaperRef.empty() ? "未启用（content/ 下没有 wallpaper.*）"
                                       : "content/" + wallpaperRef)
              << "\n";

    // 自定义域名：必须是真域名，否则 GitHub 的 CNAME 会失效
    const std::string domainRaw = ctx.count("domain") ? trim(ctx.at("domain")) : "";
    const std::string domain = looksLikeDomain(domainRaw) ? domainRaw : "";
    if (!domainRaw.empty() && domain.empty()) {
        std::cerr << "[ssg] 警告: domain 填的不是域名（" << domainRaw << "），已忽略。\n";
    }
    ctx["og_image"] = domain.empty() ? avatarRef : "https://" + domain + "/" + avatarRef;

    // 4. 进场门 + WELCOME 层
    const Digest digest = buildDigest(contentDir);
    const std::string gateHtml = buildGate(ctx, digest);
    ctx["gate"] = gateHtml;
    ctx["gate_enabled"] = gateHtml.empty() ? "false" : "true";
    ctx["welcome_layer"] = buildWelcomeLayer(ctx);

    const std::string welcomeMode = ctx.count("welcome") ? trim(ctx.at("welcome")) : "always";
    const bool welcomeAlways = (welcomeMode == "always" || welcomeMode == "on" ||
                                welcomeMode == "true" || welcomeMode == "1");
    ctx["welcome_always"] = welcomeAlways ? "true" : "false";

    const std::string motionMode = ctx.count("motion") ? trim(ctx.at("motion")) : "always";
    const bool motionAlways = (motionMode == "always" || motionMode == "on" ||
                               motionMode == "true" || motionMode == "1");
    ctx["motion_always"] = motionAlways ? "true" : "false";

    // 5. 渲染并把"素材"写成文件（全部来自 src/*.cpp 里的字符串）
    const std::string page = render(webPageHtml(), ctx);
    writeTextFile(outDir / "index.html", page);
    writeTextFile(outDir / "style.css", render(webStyleCss(), ctx));
    writeTextFile(outDir / "site.js", webScriptJs());
    writeTextFile(outDir / "fx.js", webFxJs());

    const std::string avatarSvg = buildAvatarSvg(ctx);
    writeTextFile(outDir / "avatar.svg", avatarSvg);
    writeTextFile(outDir / "favicon.svg", avatarSvg);

    if (!domain.empty()) writeTextFile(outDir / "CNAME", domain + "\n");
    writeTextFile(outDir / ".nojekyll", "");

    std::cout << "[ssg] 动效: " << (motionAlways ? "always（页面自己决定）" : "auto（跟随系统设置）")
              << "；WELCOME 动画: "
              << (welcomeAlways ? "always（强制播放）" : "auto（跟随系统设置）") << "\n";
    // 索引 = 内容条目 + 5 个区块名，数一下好对账（下面 hits 里已经含核心面板的规格）
    std::cout << "[ssg] 查找索引: " << (hits.size() + secs.size()) << " 条（内容 "
              << hits.size() << " + 区块 " << secs.size() << "）\n";
    std::cout << "[ssg] 完成 -> " << outDir.string() << "\n";
    std::cout << "[ssg] index.html " << page.size() << " 字节 / style.css "
              << webStyleCss().size() << " 字节 / site.js " << webScriptJs().size()
              << " 字节 / fx.js " << webFxJs().size() << " 字节\n";
    return 0;
}
