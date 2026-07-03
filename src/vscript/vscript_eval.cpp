#include "vscript_internal.h"

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace vscript::detail {

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

bool Truthy(const value& v)
{
    if (v.type == value::kind::boolean) return v.boolean;
    if (v.type == value::kind::number) return v.number != 0.0;
    if (v.type == value::kind::text) return !v.text.empty() && v.text != L"void";
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
    return L"void";
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
    if (v.type == value::kind::text) {
        try { return std::stod(v.text); } catch (...) { return 0.0; }
    }
    return 0.0;
}

value GetVar(const std::wstring& name)
{
    auto it = s_vars.find(name);
    if (it != s_vars.end()) return it->second;
    return TextValue(L"void");
}

value GetVarFromMap(const std::map<std::wstring, value>& vars, const std::wstring& name)
{
    auto it = vars.find(name);
    if (it != vars.end()) return it->second;
    return TextValue(L"void");
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
    for (size_t i = 0; i < script.size(); ++i) {
        wchar_t c = script[i];
        if (c == L'"' && (i == 0 || script[i - 1] != L'\\')) inString = !inString;
        if (!inString) {
            if (c == L'(') ++paren;
            if (c == L')' && paren > 0) --paren;
            if (c == L'{') ++brace;
            if (c == L'}') --brace;
            if (c == L';' && brace == 0 && paren == 0) {
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

std::vector<std::wstring> SplitArgs(const std::wstring& args)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inString = false;
    int paren = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        wchar_t c = args[i];
        if (c == L'"' && (i == 0 || args[i - 1] != L'\\')) inString = !inString;
        if (!inString) {
            if (c == L'(') ++paren;
            if (c == L')') --paren;
            if (c == L',' && paren == 0) {
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

value EvalExprWithVars(const std::wstring& expr, const std::map<std::wstring, value>& vars)
{
    std::wstring e = Trim(expr);
    auto findOp = [&](const std::wstring& ops) -> std::pair<size_t, wchar_t> {
        bool inString = false;
        int paren = 0;
        for (size_t i = e.size(); i > 0; --i) {
            size_t idx = i - 1;
            wchar_t c = e[idx];
            if (c == L'"' && (idx == 0 || e[idx - 1] != L'\\')) inString = !inString;
            if (inString) continue;
            if (c == L')') ++paren;
            else if (c == L'(' && paren > 0) --paren;
            if (paren == 0 && ops.find(c) != std::wstring::npos) {
                if (c == L'-' && (idx == 0 || std::wstring(L"+-*/(").find(e[idx - 1]) != std::wstring::npos)) continue;
                return { idx, c };
            }
        }
        return { std::wstring::npos, L'\0' };
    };
    if (e.size() >= 2 && e.front() == L'(' && e.back() == L')') {
        return EvalExprWithVars(e.substr(1, e.size() - 2), vars);
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
    auto mulOp = findOp(L"*/");
    if (mulOp.first != std::wstring::npos) {
        value left = EvalExprWithVars(e.substr(0, mulOp.first), vars);
        value right = EvalExprWithVars(e.substr(mulOp.first + 1), vars);
        double r = ToNumber(right);
        if (mulOp.second == L'/' && r == 0.0) return NumberValue(0.0);
        return NumberValue(mulOp.second == L'*' ? ToNumber(left) * r : ToNumber(left) / r);
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
    if (e == L"void") return TextValue(L"void");
    if (!e.empty() && (iswdigit(e[0]) || e[0] == L'-')) {
        try { return NumberValue(std::stod(e)); } catch (...) {}
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
    for (size_t i = 0; i + op.size() <= text.size(); ++i) {
        wchar_t c = text[i];
        if (c == L'"' && (i == 0 || text[i - 1] != L'\\')) inString = !inString;
        if (inString) continue;
        if (c == L'(') ++paren;
        else if (c == L')' && paren > 0) --paren;
        if (paren == 0 && text.compare(i, op.size(), op) == 0) return i;
    }
    return std::wstring::npos;
}

bool EvalConditionWithVars(std::wstring cond, const std::map<std::wstring, value>& vars)
{
    cond = Trim(cond);
    if (cond.empty()) return false;
    if (cond.size() >= 2 && cond.front() == L'(' && cond.back() == L')')
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
    size_t rp = s.find(L')', lp);
    size_t lb = s.find(L'{', rp);
    size_t rb = s.rfind(L'}');
    if (lp == std::wstring::npos || rp == std::wstring::npos || lb == std::wstring::npos || rb == std::wstring::npos || rb < lb) return true;
    head = s.substr(lp + 1, rp - lp - 1);
    body = s.substr(lb + 1, rb - lb - 1);
    return true;
}

bool TryExecuteIf(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    if (s.rfind(L"if", 0) != 0) return false;
    size_t lp = s.find(L'(');
    size_t rp = s.find(L')', lp);
    size_t lb = s.find(L'{', rp);
    size_t rb = s.find(L'}', lb);
    if (lp == std::wstring::npos || rp == std::wstring::npos || lb == std::wstring::npos || rb == std::wstring::npos || rb < lb) return true;
    std::wstring cond = s.substr(lp + 1, rp - lp - 1);
    std::wstring body = s.substr(lb + 1, rb - lb - 1);
    if (EvalCondition(cond)) {
        ExecuteBlock(body);
    } else {
        size_t elsePos = s.find(L"else", rb + 1);
        if (elsePos != std::wstring::npos) {
            size_t elb = s.find(L'{', elsePos);
            size_t erb = s.rfind(L'}');
            if (elb != std::wstring::npos && erb != std::wstring::npos && erb > elb) {
                ExecuteBlock(s.substr(elb + 1, erb - elb - 1));
            }
        }
    }
    return true;
}

bool TryExecuteWhile(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    std::wstring cond;
    std::wstring body;
    if (!ExtractControlBlock(s, L"while", cond, body)) return false;
    for (int i = 0; i < 1000 && EvalCondition(cond) && !s_returnRequested; ++i) {
        ExecuteBlock(body);
    }
    return true;
}

bool TryExecuteFor(const std::wstring& stmt)
{
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
    for (int i = 0; i < 1000 && !s_returnRequested; ++i) {
        if (parts.size() > 1 && !parts[1].empty() && !EvalCondition(parts[1])) break;
        ExecuteBlock(body);
        if (parts.size() > 2 && !parts[2].empty()) ExecuteStatement(parts[2]);
    }
    return true;
}

void ExecuteStatement(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    if (s.empty()) return;
    if (s_returnRequested) return;
    if (s == L"return") {
        s_returnRequested = true;
        return;
    }
    if (s.rfind(L"goto ", 0) == 0) {
        s_gotoTarget = Trim(s.substr(5));
        return;
    }
    if (TryExecuteIf(s)) return;
    if (TryExecuteWhile(s)) return;
    if (TryExecuteFor(s)) return;

    for (const auto& prefix : { L"int ", L"float ", L"string " }) {
        if (s.rfind(prefix, 0) == 0) {
            size_t eq = s.find(L'=');
            std::wstring name = Trim(s.substr(wcslen(prefix), eq == std::wstring::npos ? std::wstring::npos : eq - wcslen(prefix)));
            s_vars[name] = eq == std::wstring::npos ? value{} : EvalExpr(s.substr(eq + 1));
            return;
        }
    }
    size_t eq = s.find(L'=');
    if (eq != std::wstring::npos && s.find(L"==") == std::wstring::npos) {
        std::wstring name = Trim(s.substr(0, eq));
        s_vars[name] = EvalExpr(s.substr(eq + 1));
        return;
    }
    auto fn = ParseFunction(s);
    if (!fn) return;
    ExecuteFunction(fn->first, SplitArgs(fn->second));
}

void ExecuteBlock(const std::wstring& script)
{
    std::vector<std::wstring> statements = SplitStatements(script);
    std::unordered_map<std::wstring, size_t> labels;
    for (size_t i = 0; i < statements.size(); ++i) {
        std::wstring s = Trim(statements[i]);
        if (!s.empty() && s.back() == L':' && s.find(L' ') == std::wstring::npos) {
            labels[Trim(s.substr(0, s.size() - 1))] = i;
        }
    }
    for (size_t pc = 0; pc < statements.size() && !s_returnRequested; ++pc) {
        std::wstring s = Trim(statements[pc]);
        if (!s.empty() && s.back() == L':' && s.find(L' ') == std::wstring::npos) continue;
        ExecuteStatement(s);
        if (s_gotoTarget) {
            auto it = labels.find(*s_gotoTarget);
            s_gotoTarget.reset();
            if (it != labels.end()) pc = it->second;
        }
    }
}

} // namespace vscript::detail
