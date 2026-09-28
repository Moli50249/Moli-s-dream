#pragma once

#include <string>

namespace ssg {

// 极简 Markdown -> HTML。
// 支持：# / ## / ### 标题、- 列表、--- 分隔线、段落，
// 行内：**粗体**、`代码`、[文字](链接)。
std::string mdToHtml(const std::string& md);

// 行内标记（供调用方单独处理某一行）
std::string inlineMd(const std::string& text);

}  // namespace ssg
