#include "vscript_internal.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace vscript::detail {

value NullValue()
{
    return {};
}

value TextValue(const std::wstring& s)
{
    value v;
    v.type = value::kind::text;
    v.text = s;
    return v;
}

value NumberValue(double n)
{
    value v;
    v.type = value::kind::number;
    v.number = n;
    return v;
}

value BoolValue(bool b)
{
    value v;
    v.type = value::kind::boolean;
    v.boolean = b;
    return v;
}

value ListValue(const std::vector<value>& items)
{
    value v;
    v.type = value::kind::list;
    v.list = items;
    return v;
}

value ObjectValue(const std::map<std::wstring, value>& fields)
{
    value v;
    v.type = value::kind::object;
    v.object = fields;
    return v;
}

bool Truthy(const value& v)
{
    if (v.type == value::kind::boolean) return v.boolean;
    if (v.type == value::kind::number) return v.number != 0.0;
    if (v.type == value::kind::text) return !v.text.empty() && v.text != L"void";
    if (v.type == value::kind::list) return !v.list.empty();
    if (v.type == value::kind::object) return !v.object.empty();
    return false;
}

std::wstring ToText(const value& v)
{
    if (v.type == value::kind::text) return v.text;
    if (v.type == value::kind::boolean) return v.boolean ? L"true" : L"false";
    if (v.type == value::kind::number) {
        std::wostringstream oss;
        oss << v.number;
        return oss.str();
    }
    if (v.type == value::kind::list) {
        std::wstring out = L"{";
        for (size_t i = 0; i < v.list.size(); ++i) {
            if (i != 0) out += L", ";
            out += ToText(v.list[i]);
        }
        out += L"}";
        return out;
    }
    if (v.type == value::kind::object) {
        std::wstring out = L"{";
        bool first = true;
        for (const auto& [key, item] : v.object) {
            if (!first) out += L", ";
            first = false;
            out += key + L": " + ToText(item);
        }
        out += L"}";
        return out;
    }
    return L"";
}

std::wstring ExpandEnvText(const std::wstring& text)
{
    DWORD need = ExpandEnvironmentStringsW(text.c_str(), nullptr, 0);
    if (need == 0) return text;
    std::wstring out(need, L'\0');
    ExpandEnvironmentStringsW(text.c_str(), out.data(), need);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

double ToNumber(const value& v)
{
    if (v.type == value::kind::number) return v.number;
    if (v.type == value::kind::boolean) return v.boolean ? 1.0 : 0.0;
    if (v.type == value::kind::list) return static_cast<double>(v.list.size());
    if (v.type == value::kind::object) return static_cast<double>(v.object.size());
    if (v.type == value::kind::text) {
        try { return std::stod(v.text); } catch (...) { return 0.0; }
    }
    return 0.0;
}

value GetVar(const std::wstring& name)
{
    execution_context& exec = CurrentExecution();
    for (auto it = exec.localScopes.rbegin(); it != exec.localScopes.rend(); ++it) {
        auto local = it->find(name);
        if (local != it->end()) return local->second;
    }
    auto it = s_vars.find(name);
    if (it != s_vars.end()) return it->second;
    return NullValue();
}

value GetVarFromMap(const std::map<std::wstring, value>& vars, const std::wstring& name)
{
    auto it = vars.find(name);
    if (it != vars.end()) return it->second;
    return GetVar(name);
}

std::wstring Trim(std::wstring s)
{
    auto isSpace = [](wchar_t c) { return iswspace(c) != 0; };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), isSpace));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), isSpace).base(), s.end());
    return s;
}

std::vector<std::wstring> SplitStatements(const std::wstring& script)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inString = false;
    int brace = 0;
    int paren = 0;
    auto pushCurrent = [&]() {
        const std::wstring trimmed = Trim(cur);
        if (!trimmed.empty()) out.push_back(trimmed);
        cur.clear();
    };
    for (size_t i = 0; i < script.size(); ++i) {
        wchar_t c = script[i];
        if (c == L'"' && (i == 0 || script[i - 1] != L'\\')) inString = !inString;
        if (!inString) {
            if (c == L'(') ++paren;
            if (c == L')' && paren > 0) --paren;
            if (c == L'{') ++brace;
            if (c == L'}') --brace;
            if (c == L';' && brace == 0 && paren == 0) {
                pushCurrent();
                continue;
            }
        }
        cur.push_back(c);
        if (!inString && c == L'}' && brace == 0 && paren == 0) {
            size_t j = i + 1;
            while (j < script.size() && iswspace(script[j]) != 0) ++j;
            if (j < script.size() && script[j] == L';') continue;
            if (j + 3 < script.size() && script.compare(j, 4, L"else") == 0) continue;
            pushCurrent();
        }
    }
    pushCurrent();
    return out;
}

std::vector<std::wstring> SplitArgs(const std::wstring& args)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inString = false;
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        wchar_t c = args[i];
        if (c == L'"' && (i == 0 || args[i - 1] != L'\\')) inString = !inString;
        if (!inString) {
            if (c == L'(') ++paren;
            if (c == L')') --paren;
            if (c == L'{') ++brace;
            if (c == L'}') --brace;
            if (c == L'[') ++bracket;
            if (c == L']') --bracket;
            if (c == L',' && paren == 0 && brace == 0 && bracket == 0) {
                out.push_back(Trim(cur));
                cur.clear();
                continue;
            }
        }
        cur.push_back(c);
    }
    if (!Trim(cur).empty()) out.push_back(Trim(cur));
    return out;
}

std::optional<std::pair<std::wstring, std::wstring>> ParseIndexAccess(const std::wstring& expr)
{
    const std::wstring e = Trim(expr);
    const size_t lb = e.find(L'[');
    const size_t rb = e.rfind(L']');
    if (lb == std::wstring::npos || rb == std::wstring::npos || rb <= lb || rb != e.size() - 1) return std::nullopt;
    return std::make_pair(Trim(e.substr(0, lb)), e.substr(lb + 1, rb - lb - 1));
}

size_t FindMatchingToken(const std::wstring& text, size_t openPos, wchar_t openToken, wchar_t closeToken)
{
    bool inString = false;
    int depth = 0;
    for (size_t i = openPos; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if (c == L'"' && (i == 0 || text[i - 1] != L'\\')) inString = !inString;
        if (inString) continue;
        if (c == openToken) ++depth;
        else if (c == closeToken) {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::wstring::npos;
}

size_t SkipSpacesForward(const std::wstring& text, size_t start)
{
    size_t i = start;
    while (i < text.size() && iswspace(text[i]) != 0) ++i;
    return i;
}

std::optional<value> ParseListLiteral(const std::wstring& expr, const std::map<std::wstring, value>& vars)
{
    const std::wstring e = Trim(expr);
    if (e.size() < 2 || e.front() != L'{' || e.back() != L'}') return std::nullopt;
    std::vector<value> items;
    for (const auto& item : SplitArgs(e.substr(1, e.size() - 2))) {
        items.push_back(EvalExprWithVars(item, vars));
    }
    return ListValue(items);
}

bool TryParseTypedDeclaration(const std::wstring& stmt, std::wstring& typeName, std::wstring& varName, std::wstring& initializer)
{
    const std::wstring s = Trim(stmt);
    const size_t eq = s.find(L'=');
    std::wstring left = Trim(eq == std::wstring::npos ? s : s.substr(0, eq));
    initializer = eq == std::wstring::npos ? L"" : Trim(s.substr(eq + 1));
    const bool isConst = left.rfind(L"const ", 0) == 0;
    if (isConst) left = Trim(left.substr(6));

    for (const auto& prefix : { L"int ", L"float ", L"double ", L"string ", L"bool ", L"auto " }) {
        if (left.rfind(prefix, 0) == 0) {
            typeName = Trim(std::wstring(prefix).substr(0, std::wstring(prefix).size() - 1));
            if (isConst) typeName = L"const " + typeName;
            varName = Trim(left.substr(wcslen(prefix)));
            return !varName.empty();
        }
    }

    for (const auto& containerPrefix : { L"vector<", L"array<" }) {
        if (left.rfind(containerPrefix, 0) != 0) continue;
        int depth = 0;
        size_t closePos = std::wstring::npos;
        for (size_t i = 0; i < left.size(); ++i) {
            if (left[i] == L'<') ++depth;
            else if (left[i] == L'>') {
                --depth;
                if (depth == 0) {
                    closePos = i;
                    break;
                }
            }
        }
        if (closePos == std::wstring::npos || closePos + 1 >= left.size()) return false;
        typeName = Trim(left.substr(0, closePos + 1));
        if (isConst) typeName = L"const " + typeName;
        varName = Trim(left.substr(closePos + 1));
        return !varName.empty();
    }
    return false;
}

std::optional<std::pair<std::wstring, std::wstring>> ParseCompoundAssignment(const std::wstring& stmt)
{
    for (const auto& op : { L"+=", L"-=", L"*=", L"/=", L"%=" }) {
        const size_t pos = stmt.find(op);
        if (pos == std::wstring::npos) continue;
        return std::make_pair(Trim(stmt.substr(0, pos)), std::wstring(op));
    }
    return std::nullopt;
}

std::optional<std::pair<std::wstring, int>> ParseIncDecStatement(const std::wstring& stmt)
{
    const std::wstring s = Trim(stmt);
    if (s.size() > 2 && s.substr(s.size() - 2) == L"++") return std::make_pair(Trim(s.substr(0, s.size() - 2)), 1);
    if (s.size() > 2 && s.substr(s.size() - 2) == L"--") return std::make_pair(Trim(s.substr(0, s.size() - 2)), -1);
    if (s.rfind(L"++", 0) == 0) return std::make_pair(Trim(s.substr(2)), 1);
    if (s.rfind(L"--", 0) == 0) return std::make_pair(Trim(s.substr(2)), -1);
    return std::nullopt;
}

value CoerceValueForType(const value& input, const std::wstring& typeName)
{
    if (typeName == L"int" || typeName == L"float" || typeName == L"double") return NumberValue(ToNumber(input));
    if (typeName == L"auto") return input;
    if (typeName == L"bool") return BoolValue(Truthy(input));
    if (typeName == L"string") return TextValue(ToText(input));
    if (typeName.rfind(L"vector<", 0) == 0 || typeName.rfind(L"array<", 0) == 0) {
        if (input.type == value::kind::list) return input;
        return ListValue({ input });
    }
    if (typeName.rfind(L"object", 0) == 0 && input.type == value::kind::object) return input;
    if (typeName == L"void") return NullValue();
    return input;
}

std::optional<value> TryResolveAccessorExpression(const std::wstring& expr, const std::map<std::wstring, value>& vars)
{
    const std::wstring e = Trim(expr);
    if (e.empty()) return std::nullopt;

    bool inString = false;
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    size_t firstAccessor = std::wstring::npos;
    for (size_t i = 0; i < e.size(); ++i) {
        const wchar_t c = e[i];
        if (c == L'"' && (i == 0 || e[i - 1] != L'\\')) inString = !inString;
        if (inString) continue;
        if (c == L'(') ++paren;
        else if (c == L')' && paren > 0) --paren;
        else if (c == L'{') ++brace;
        else if (c == L'}' && brace > 0) --brace;
        else if (c == L'[') {
            if (paren == 0 && brace == 0 && bracket == 0) {
                firstAccessor = i;
                break;
            }
            ++bracket;
        } else if (c == L']' && bracket > 0) {
            --bracket;
        } else if (c == L'.' && paren == 0 && brace == 0 && bracket == 0) {
            firstAccessor = i;
            break;
        }
    }
    if (firstAccessor == std::wstring::npos) return std::nullopt;

    value current = EvalExprWithVars(e.substr(0, firstAccessor), vars);
    size_t i = firstAccessor;
    while (i < e.size()) {
        if (e[i] == L'.') {
            ++i;
            size_t start = i;
            while (i < e.size() && (iswalnum(e[i]) != 0 || e[i] == L'_')) ++i;
            const std::wstring member = e.substr(start, i - start);
            if (member.empty()) return TextValue(L"void");
            if (member == L"size") {
                if (current.type == value::kind::list) current = NumberValue((double)current.list.size());
                else if (current.type == value::kind::object) current = NumberValue((double)current.object.size());
                else if (current.type == value::kind::text) current = NumberValue((double)current.text.size());
                else current = NumberValue(ToNumber(current));
                continue;
            }
            if (current.type != value::kind::object) return TextValue(L"void");
            auto it = current.object.find(member);
            if (it == current.object.end()) return TextValue(L"void");
            current = it->second;
            continue;
        }
        if (e[i] == L'[') {
            const size_t close = FindMatchingToken(e, i, L'[', L']');
            if (close == std::wstring::npos) return TextValue(L"void");
            const std::wstring inner = e.substr(i + 1, close - i - 1);
            const value key = EvalExprWithVars(inner, vars);
            if (current.type == value::kind::list) {
                const int index = (int)ToNumber(key);
                if (index < 0 || static_cast<size_t>(index) >= current.list.size()) return TextValue(L"void");
                current = current.list[(size_t)index];
            } else if (current.type == value::kind::object) {
                const std::wstring member = ToText(key);
                auto it = current.object.find(member);
                if (it == current.object.end()) return TextValue(L"void");
                current = it->second;
            } else if (current.type == value::kind::text) {
                const int index = (int)ToNumber(key);
                if (index < 0 || static_cast<size_t>(index) >= current.text.size()) return TextValue(L"void");
                current = TextValue(std::wstring(1, current.text[(size_t)index]));
            } else {
                return TextValue(L"void");
            }
            i = close + 1;
            continue;
        }
        ++i;
    }
    return current;
}

bool TryParseScriptFunctionDefinition(const std::wstring& stmt, std::wstring& functionName, std::wstring& functionBody)
{
    const std::wstring s = Trim(stmt);
    size_t bracePos = s.find(L'{');
    size_t endBracePos = s.rfind(L'}');
    if (bracePos == std::wstring::npos || endBracePos == std::wstring::npos || endBracePos <= bracePos) return false;

    const size_t parenPos = s.find(L'(');
    const size_t closeParenPos = s.rfind(L')');
    if (parenPos == std::wstring::npos || closeParenPos == std::wstring::npos || closeParenPos < parenPos || closeParenPos > bracePos) return false;

    const std::wstring header = Trim(s.substr(0, bracePos));
    const auto IsSupportedReturnType = [](const std::wstring& text) {
        for (const auto& prefix : { L"int ", L"float ", L"double ", L"string ", L"bool ", L"void ", L"auto " }) {
            if (text.rfind(prefix, 0) == 0) return true;
        }
        return text.rfind(L"vector<", 0) == 0 || text.rfind(L"array<", 0) == 0;
    };
    if (!IsSupportedReturnType(header)) return false;

    const std::wstring name = Trim(header.substr(0, parenPos));
    const size_t lastSpace = name.find_last_of(L' ');
    if (lastSpace == std::wstring::npos) return false;

    functionName = Trim(name.substr(lastSpace + 1));
    functionBody = s.substr(bracePos + 1, endBracePos - bracePos - 1);
    return !functionName.empty();
}

std::vector<std::wstring> ParseScriptFunctionParams(const std::wstring& headerArgs)
{
    std::vector<std::wstring> params;
    for (const auto& rawParam : SplitArgs(headerArgs)) {
        const std::wstring param = Trim(rawParam);
        if (param.empty()) continue;
        const size_t lastSpace = param.find_last_of(L' ');
        params.push_back(lastSpace == std::wstring::npos ? param : Trim(param.substr(lastSpace + 1)));
    }
    return params;
}

value EvalExprWithVars(const std::wstring& expr, const std::map<std::wstring, value>& vars)
{
    std::wstring e = Trim(expr);
    auto findOp = [&](const std::wstring& ops) -> std::pair<size_t, wchar_t> {
        bool inString = false;
        int paren = 0;
        int brace = 0;
        int bracket = 0;
        for (size_t i = e.size(); i > 0; --i) {
            size_t idx = i - 1;
            wchar_t c = e[idx];
            if (c == L'"' && (idx == 0 || e[idx - 1] != L'\\')) inString = !inString;
            if (inString) continue;
            if (c == L')') ++paren;
            else if (c == L'(' && paren > 0) --paren;
            else if (c == L'}') ++brace;
            else if (c == L'{' && brace > 0) --brace;
            else if (c == L']') ++bracket;
            else if (c == L'[' && bracket > 0) --bracket;
            if (paren == 0 && brace == 0 && bracket == 0 && ops.find(c) != std::wstring::npos) {
                if (c == L'-' && (idx == 0 || std::wstring(L"+-*/(").find(e[idx - 1]) != std::wstring::npos)) continue;
                return { idx, c };
            }
        }
        return { std::wstring::npos, L'\0' };
    };
    if (e.size() >= 2 && e.front() == L'(' && e.back() == L')' &&
        FindMatchingToken(e, 0, L'(', L')') == e.size() - 1) {
        return EvalExprWithVars(e.substr(1, e.size() - 2), vars);
    }
    if (e.rfind(L"on:", 0) == 0) {
        return BoolValue(EvalCondition(e));
    }
    if (e.rfind(L"!", 0) == 0 ||
        FindLogicalOp(e, L"||") != std::wstring::npos ||
        FindLogicalOp(e, L"&&") != std::wstring::npos ||
        FindLogicalOp(e, L">=") != std::wstring::npos ||
        FindLogicalOp(e, L"<=") != std::wstring::npos ||
        FindLogicalOp(e, L"==") != std::wstring::npos ||
        FindLogicalOp(e, L"!=") != std::wstring::npos ||
        FindLogicalOp(e, L">") != std::wstring::npos ||
        FindLogicalOp(e, L"<") != std::wstring::npos) {
        return BoolValue(EvalConditionWithVars(e, vars));
    }
    if (auto list = ParseListLiteral(e, vars)) {
        return *list;
    }
    auto addOp = findOp(L"+-");
    if (addOp.first != std::wstring::npos) {
        value left = EvalExprWithVars(e.substr(0, addOp.first), vars);
        value right = EvalExprWithVars(e.substr(addOp.first + 1), vars);
        if (addOp.second == L'+' && (left.type == value::kind::text || right.type == value::kind::text)) {
            return TextValue(ToText(left) + ToText(right));
        }
        return NumberValue(addOp.second == L'+' ? ToNumber(left) + ToNumber(right) : ToNumber(left) - ToNumber(right));
    }
    auto mulOp = findOp(L"*/%");
    if (mulOp.first != std::wstring::npos) {
        value left = EvalExprWithVars(e.substr(0, mulOp.first), vars);
        value right = EvalExprWithVars(e.substr(mulOp.first + 1), vars);
        double r = ToNumber(right);
        if (mulOp.second == L'/' && r == 0.0) return NumberValue(0.0);
        if (mulOp.second == L'*') return NumberValue(ToNumber(left) * r);
        if (mulOp.second == L'%') return NumberValue(r == 0.0 ? 0.0 : std::fmod(ToNumber(left), r));
        return NumberValue(ToNumber(left) / r);
    }
    if (e.size() >= 2 && e.front() == L'"' && e.back() == L'"') {
        std::wstring text;
        for (size_t i = 1; i + 1 < e.size(); ++i) {
            if (e[i] == L'\\' && i + 1 < e.size()) {
                ++i;
                if (e[i] == L'n') text.push_back(L'\n');
                else text.push_back(e[i]);
            } else {
                text.push_back(e[i]);
            }
        }
        return TextValue(text);
    }
    if (e == L"true") return BoolValue(true);
    if (e == L"false") return BoolValue(false);
    if (e == L"void" || e == L"NULL" || e == L"null" || e == L"nullptr") return NullValue();
    if (!e.empty() && (iswdigit(e[0]) || e[0] == L'-')) {
        try { return NumberValue(std::stod(e)); } catch (...) {}
    }
    if (auto accessor = TryResolveAccessorExpression(e, vars)) {
        return *accessor;
    }
    if (auto index = ParseIndexAccess(e)) {
        value base = GetVarFromMap(vars, index->first);
        const int i = static_cast<int>(ToNumber(EvalExprWithVars(index->second, vars)));
        if (base.type == value::kind::list && i >= 0 && static_cast<size_t>(i) < base.list.size()) return base.list[static_cast<size_t>(i)];
        return NullValue();
    }
    auto fn = ParseFunction(e);
    if (fn && fn->first.find(L' ') == std::wstring::npos) {
        return ExecuteFunction(fn->first, SplitArgs(fn->second));
    }
    return GetVarFromMap(vars, e);
}

value EvalExpr(const std::wstring& expr)
{
    return EvalExprWithVars(expr, s_vars);
}

bool CompareValues(const value& l, const std::wstring& op, const value& r)
{
    if (op == L"==" || op == L"=") return ToText(l) == ToText(r);
    if (op == L"!=") return ToText(l) != ToText(r);
    double a = ToNumber(l);
    double b = ToNumber(r);
    if (op == L">") return a > b;
    if (op == L"<") return a < b;
    if (op == L">=") return a >= b;
    if (op == L"<=") return a <= b;
    return false;
}

size_t FindLogicalOp(const std::wstring& text, const std::wstring& op)
{
    bool inString = false;
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    for (size_t i = 0; i + op.size() <= text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'"' && (i == 0 || text[i - 1] != L'\\')) inString = !inString;
        if (inString) continue;
        if (c == L'(') ++paren;
        else if (c == L')' && paren > 0) --paren;
        else if (c == L'{') ++brace;
        else if (c == L'}' && brace > 0) --brace;
        else if (c == L'[') ++bracket;
        else if (c == L']' && bracket > 0) --bracket;
        if (paren == 0 && brace == 0 && bracket == 0 && text.compare(i, op.size(), op) == 0) return i;
    }
    return std::wstring::npos;
}

bool EvalConditionWithVars(std::wstring cond, const std::map<std::wstring, value>& vars)
{
    cond = Trim(cond);
    if (cond.empty()) return false;
    if (cond.size() >= 2 && cond.front() == L'(' && cond.back() == L')' &&
        FindMatchingToken(cond, 0, L'(', L')') == cond.size() - 1)
        return EvalConditionWithVars(cond.substr(1, cond.size() - 2), vars);
    if (cond.rfind(L"!", 0) == 0) return !EvalConditionWithVars(cond.substr(1), vars);
    size_t orPos = FindLogicalOp(cond, L"||");
    if (orPos != std::wstring::npos)
        return EvalConditionWithVars(cond.substr(0, orPos), vars) || EvalConditionWithVars(cond.substr(orPos + 2), vars);
    size_t andPos = FindLogicalOp(cond, L"&&");
    if (andPos != std::wstring::npos)
        return EvalConditionWithVars(cond.substr(0, andPos), vars) && EvalConditionWithVars(cond.substr(andPos + 2), vars);

    static const std::vector<std::wstring> ops = { L">=", L"<=", L"==", L"!=", L">", L"<", L"=" };
    for (const auto& op : ops) {
        size_t pos = FindLogicalOp(cond, op);
        if (pos == std::wstring::npos) continue;
        std::wstring leftName = Trim(cond.substr(0, pos));
        value left = EvalExprWithVars(leftName, vars);
        value right = EvalExprWithVars(cond.substr(pos + op.size()), vars);
        return CompareValues(left, op, right);
    }
    return Truthy(EvalExprWithVars(cond, vars));
}

bool EvalCondition(std::wstring cond)
{
    cond = Trim(cond);
    bool edge = false;
    if (cond.rfind(L"on:", 0) == 0) {
        edge = true;
        cond = cond.substr(3);
    }
    bool now = EvalConditionWithVars(cond, s_vars);
    if (!edge) return now;

    // Changed/ChangedTo 本身已经比较了当前帧与上一帧。再次用 on: 包裹时，
    // 旧实现会用同一组全局快照计算两遍，导致 before 与 now 恒等，条件永远为假。
    // 将这类条件视为已经完成边沿检测，兼容 on:Changed(...) 的直观写法。
    if (cond.find(L"Changed(") != std::wstring::npos ||
        cond.find(L"ChangedTo(") != std::wstring::npos) {
        return now;
    }

    bool before = EvalConditionWithVars(cond, s_prevVars);
    return now && !before;
}

std::optional<std::pair<std::wstring, std::wstring>> ParseFunction(const std::wstring& stmt)
{
    size_t p = stmt.find(L'(');
    size_t q = stmt.rfind(L')');
    if (p == std::wstring::npos || q == std::wstring::npos || q < p) return std::nullopt;
    return std::make_pair(Trim(stmt.substr(0, p)), stmt.substr(p + 1, q - p - 1));
}

bool ExtractControlBlock(const std::wstring& s, const std::wstring& keyword, std::wstring& head, std::wstring& body)
{
    if (s.rfind(keyword, 0) != 0) return false;
    size_t lp = s.find(L'(');
    size_t rp = lp == std::wstring::npos ? std::wstring::npos : FindMatchingToken(s, lp, L'(', L')');
    size_t lb = s.find(L'{', rp);
    size_t rb = lb == std::wstring::npos ? std::wstring::npos : FindMatchingToken(s, lb, L'{', L'}');
    if (lp == std::wstring::npos || rp == std::wstring::npos || lb == std::wstring::npos || rb == std::wstring::npos || rb < lb) return true;
    head = s.substr(lp + 1, rp - lp - 1);
    body = s.substr(lb + 1, rb - lb - 1);
    return true;
}

bool TryExecuteIf(const std::wstring& stmt)
{
    execution_context& exec = CurrentExecution();
    std::wstring s = Trim(stmt);
    if (s.rfind(L"if", 0) != 0) return false;
    size_t cursor = 0;
    while (cursor < s.size()) {
        const bool isElseIf = s.compare(cursor, 4, L"else") == 0 || s.compare(cursor, 6, L"elseif") == 0;
        const bool isIf = !isElseIf && s.compare(cursor, 2, L"if") == 0;
        if (!isIf && !isElseIf) return true;

        size_t clauseStart = cursor;
        if (isElseIf) {
            if (s.compare(cursor, 6, L"elseif") == 0) {
                cursor = cursor + 4;
            } else {
                cursor = SkipSpacesForward(s, cursor + 4);
            }
            if (s.compare(cursor, 2, L"if") != 0) {
                size_t elseBodyStart = s.find(L'{', cursor);
                size_t elseBodyEnd = elseBodyStart == std::wstring::npos ? std::wstring::npos : FindMatchingToken(s, elseBodyStart, L'{', L'}');
                if (elseBodyStart == std::wstring::npos || elseBodyEnd == std::wstring::npos) return true;
                ExecuteBlock(s.substr(elseBodyStart + 1, elseBodyEnd - elseBodyStart - 1));
                return true;
            }
        }

        const size_t ifPos = isElseIf ? cursor : clauseStart;
        const size_t lp = s.find(L'(', ifPos);
        const size_t rp = lp == std::wstring::npos ? std::wstring::npos : FindMatchingToken(s, lp, L'(', L')');
        const size_t lb = s.find(L'{', rp);
        const size_t rb = lb == std::wstring::npos ? std::wstring::npos : FindMatchingToken(s, lb, L'{', L'}');
        if (lp == std::wstring::npos || rp == std::wstring::npos || lb == std::wstring::npos || rb == std::wstring::npos) return true;

        const std::wstring cond = s.substr(lp + 1, rp - lp - 1);
        const std::wstring body = s.substr(lb + 1, rb - lb - 1);
        if (EvalCondition(cond)) {
            ExecuteBlock(body);
            return true;
        }
        cursor = SkipSpacesForward(s, rb + 1);
        if (s.compare(cursor, 4, L"else") != 0) return true;
    }
    return true;
}

bool TryExecuteWhile(const std::wstring& stmt)
{
    execution_context& exec = CurrentExecution();
    std::wstring s = Trim(stmt);
    std::wstring cond;
    std::wstring body;
    if (!ExtractControlBlock(s, L"while", cond, body)) return false;
    for (int i = 0; i < 1000 && EvalCondition(cond) && !exec.returnRequested; ++i) {
        ExecuteBlock(body);
        if (exec.gotoTarget || exec.breakRequested) {
            exec.breakRequested = false;
            break;
        }
        if (exec.continueRequested) {
            exec.continueRequested = false;
            continue;
        }
    }
    return true;
}

bool TryExecuteFor(const std::wstring& stmt)
{
    execution_context& exec = CurrentExecution();
    std::wstring s = Trim(stmt);
    std::wstring head;
    std::wstring body;
    if (!ExtractControlBlock(s, L"for", head, body)) return false;
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : head) {
        if (c == L';') {
            parts.push_back(Trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(Trim(cur));
    if (!parts.empty()) ExecuteStatement(parts[0]);
    for (int i = 0; i < 1000 && !exec.returnRequested; ++i) {
        if (parts.size() > 1 && !parts[1].empty() && !EvalCondition(parts[1])) break;
        ExecuteBlock(body);
        if (exec.gotoTarget || exec.breakRequested) {
            exec.breakRequested = false;
            break;
        }
        if (exec.continueRequested) {
            exec.continueRequested = false;
        }
        if (parts.size() > 2 && !parts[2].empty()) ExecuteStatement(parts[2]);
    }
    return true;
}

void AssignValue(const std::wstring& name, const value& assigned)
{
    execution_context& exec = CurrentExecution();
    if (IsConstVariable(name)) {
        std::wcout << L"[脚本] 检测到 const 变量写入请求，已忽略: " << name << std::endl;
        return;
    }
    for (auto it = exec.localScopes.rbegin(); it != exec.localScopes.rend(); ++it) {
        auto local = it->find(name);
        if (local != it->end()) {
            local->second = assigned;
            std::wcout << L"[脚本] 更新局部变量: " << name << L" = " << ToText(assigned) << std::endl;
            return;
        }
    }
    s_vars[name] = assigned;
    std::wcout << L"[脚本] 更新全局变量: " << name << L" = " << ToText(assigned) << std::endl;
}

void ExecuteStatement(const std::wstring& stmt)
{
    execution_context& exec = CurrentExecution();
    std::wstring s = Trim(stmt);
    if (s.empty()) return;
    if (exec.returnRequested) return;
    if (s == L"break") {
        exec.breakRequested = true;
        std::wcout << L"[脚本] 遇到 break" << std::endl;
        return;
    }
    if (s == L"continue") {
        exec.continueRequested = true;
        std::wcout << L"[脚本] 遇到 continue" << std::endl;
        return;
    }
    if (s == L"return") {
        exec.returnRequested = true;
        exec.returnValue = TextValue(L"void");
        std::wcout << L"[脚本] 遇到 return，返回 void" << std::endl;
        return;
    }
    if (s.rfind(L"return ", 0) == 0) {
        exec.returnRequested = true;
        exec.returnValue = EvalExpr(s.substr(7));
        std::wcout << L"[脚本] 遇到 return，返回值=" << ToText(*exec.returnValue) << std::endl;
        return;
    }
    if (s.rfind(L"goto ", 0) == 0) {
        exec.gotoTarget = Trim(s.substr(5));
        return;
    }
    if (TryExecuteIf(s)) return;
    if (TryExecuteWhile(s)) return;
    if (TryExecuteFor(s)) return;

    std::wstring functionName;
    std::wstring functionBody;
    if (TryParseScriptFunctionDefinition(s, functionName, functionBody)) return;

    std::wstring declaredType;
    std::wstring declaredName;
    std::wstring declaredInit;
    if (TryParseTypedDeclaration(s, declaredType, declaredName, declaredInit)) {
        const bool isConst = declaredType.rfind(L"const ", 0) == 0;
        const std::wstring storageType = isConst ? Trim(declaredType.substr(6)) : declaredType;
        value assigned = declaredInit.empty() ? value{} : CoerceValueForType(EvalExpr(declaredInit), storageType);
        if (!exec.localScopes.empty()) exec.localScopes.back()[declaredName] = assigned;
        else s_vars[declaredName] = assigned;
        if (isConst) RegisterConstVariable(declaredName);
        return;
    }
    if (auto incDec = ParseIncDecStatement(s)) {
        AssignValue(incDec->first, NumberValue(ToNumber(GetVar(incDec->first)) + incDec->second));
        return;
    }
    if (auto compound = ParseCompoundAssignment(s)) {
        const size_t pos = s.find(compound->second);
        const value left = GetVar(compound->first);
        const value right = EvalExpr(s.substr(pos + compound->second.size()));
        if (compound->second == L"+=") AssignValue(compound->first, left.type == value::kind::text || right.type == value::kind::text ? TextValue(ToText(left) + ToText(right)) : NumberValue(ToNumber(left) + ToNumber(right)));
        else if (compound->second == L"-=") AssignValue(compound->first, NumberValue(ToNumber(left) - ToNumber(right)));
        else if (compound->second == L"*=") AssignValue(compound->first, NumberValue(ToNumber(left) * ToNumber(right)));
        else if (compound->second == L"/=") AssignValue(compound->first, NumberValue(ToNumber(right) == 0.0 ? 0.0 : ToNumber(left) / ToNumber(right)));
        else if (compound->second == L"%=") AssignValue(compound->first, NumberValue(ToNumber(right) == 0.0 ? 0.0 : std::fmod(ToNumber(left), ToNumber(right))));
        return;
    }

    for (const auto& prefix : { L"int ", L"float ", L"string ", L"bool " }) {
        if (s.rfind(prefix, 0) == 0) {
            size_t eq = s.find(L'=');
            std::wstring name = Trim(s.substr(wcslen(prefix), eq == std::wstring::npos ? std::wstring::npos : eq - wcslen(prefix)));
            const std::wstring typeName = Trim(std::wstring(prefix).substr(0, std::wstring(prefix).size() - 1));
            value assigned = eq == std::wstring::npos ? value{} : CoerceValueForType(EvalExpr(s.substr(eq + 1)), typeName);
            if (!exec.localScopes.empty()) {
                exec.localScopes.back()[name] = assigned;
                std::wcout << L"[脚本] 声明局部变量: " << name << L" = " << ToText(assigned) << std::endl;
            } else {
                s_vars[name] = assigned;
                std::wcout << L"[脚本] 声明全局变量: " << name << L" = " << ToText(assigned) << std::endl;
            }
            return;
        }
    }
    size_t eq = s.find(L'=');
    if (eq != std::wstring::npos && s.find(L"==") == std::wstring::npos) {
        std::wstring name = Trim(s.substr(0, eq));
        AssignValue(name, EvalExpr(s.substr(eq + 1)));
        return;
    }
    auto fn = ParseFunction(s);
    if (!fn) return;
    (void)ExecuteFunction(fn->first, SplitArgs(fn->second));
}

void ExecuteBlock(const std::wstring& script)
{
    execution_context& exec = CurrentExecution();
    std::vector<std::wstring> statements = SplitStatements(script);
    std::unordered_map<std::wstring, size_t> labels;
    for (size_t i = 0; i < statements.size(); ++i) {
        std::wstring s = Trim(statements[i]);
        if (!s.empty() && s.back() == L':' && s.find(L' ') == std::wstring::npos) {
            labels[Trim(s.substr(0, s.size() - 1))] = i;
        }
        std::wstring functionName;
        std::wstring functionBody;
        if (TryParseScriptFunctionDefinition(s, functionName, functionBody)) {
            exec.functions[functionName] = s;
            std::wcout << L"[脚本] 注册脚本函数: " << functionName << std::endl;
        }
    }
    for (size_t pc = 0; pc < statements.size() && !exec.returnRequested; ++pc) {
        std::wstring s = Trim(statements[pc]);
        if (!s.empty() && s.back() == L':' && s.find(L' ') == std::wstring::npos) continue;
        std::wstring functionName;
        std::wstring functionBody;
        if (TryParseScriptFunctionDefinition(s, functionName, functionBody)) continue;
        ExecuteStatement(s);
        if (exec.gotoTarget) {
            auto it = labels.find(*exec.gotoTarget);
            if (it != labels.end()) {
                exec.gotoTarget.reset();
                pc = it->second;
            } else {
                break;
            }
        }
        if (exec.breakRequested || exec.continueRequested) break;
    }
}

} // namespace vscript::detail
