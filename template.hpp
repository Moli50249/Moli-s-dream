#pragma once

#include <string>
#include <unordered_map>

namespace ssg {

// 模板上下文：key -> 值
using Context = std::unordered_map<std::string, std::string>;

// 把 {{key}} 替换为 ctx[key]；未命中的占位符会被清空。
std::string render(const std::string& tpl, const Context& ctx);

}  // namespace ssg
