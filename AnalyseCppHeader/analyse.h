#ifndef ANALYSE_H
#define ANALYSE_H
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "setting.h"
enum class LimitType
{
    ePublic,    // 公开
    eProtected, // 保护
    ePrivate,   // 私有
};

class FunctionInfo
{
public:
    std::string _name;                        // 函数名
    std::string _return;                      // 返回值
    std::string _params;                      // 参数集合
    LimitType   _limit = LimitType::ePrivate; // 函数权限
    int         _flag  = 0;                   // 修饰符...

    FunctionInfo(LimitType limit) : _limit(limit) {}

    enum Flag : unsigned int
    {
        eUnknownFlag      = 0,
        eConstFunction    = 1,
        eInlineFunction   = eConstFunction << 1,
        eStaticFunction   = eConstFunction << 2,
        eVirtualFunction  = eConstFunction << 3,
        eOverrideFunction = eConstFunction << 4,
        eExplicitFunction = eConstFunction << 5,
    };
    void addFlag(Flag flag) { _flag |= flag; }
    void removeFlag(Flag flag) { _flag &= (~flag); }
    bool hasFlag(Flag flag) const { return _flag & flag; }

    std::string simplifyFun() const;
    std::string simplifyParams() const; // 获取不带参数名的_params
};
using FunctionInfoSp = std::shared_ptr<FunctionInfo>;

class VariableInfo
{
public:
    std::string _type;    // 类型
    std::string _name;    // 名称
    std::string _default; // 初始化值，如有
    int         _flag  = 0;
    LimitType   _limit = LimitType::ePrivate;

    VariableInfo(LimitType limit) : _limit(limit) {}

    enum Flag : unsigned int
    {
        eUnknownFlag    = 0,
        eConstVariable  = 1,
        eStaticVariable = eConstVariable << 1,
    };
    void addFlag(Flag flag) { _flag |= flag; }
    void removeFlag(Flag flag) { _flag &= (~flag); }
    bool hasFlag(Flag flag) const { return _flag & flag; }
};
using VariableInfoSp = std::shared_ptr<VariableInfo>;

class ClassInfo
{
public:
    std::vector<FunctionInfoSp> _func;      // 函数
    std::vector<VariableInfoSp> _member;    // 成员变量
    std::string                 _className; // 类名

    void funcNameCount(std::map<std::string, int>& names) const;
};
using ClassInfoSp = std::shared_ptr<ClassInfo>;

/*
 * @brief 解析C++头文件中的类名、函数
 * @note 假设输入的文件内容是通过过C++编译器编译的，最好经过clang-format格式化
 * @todo 对模板类、模板函数的分析非常不完善，没有解析模板类型
 */
class AnalyseHeader
{
    std::string                        _contents; // 输入文件
    SettingSp                          _setting;  // 配置
    std::map<std::string, std::string> _macro;    // 宏替换，"#define" "#typedef" "using"

    ClassInfoSp              _info;    // 当前正在解析的类信息
    std::vector<ClassInfoSp> _classes; // 解析出的类信息

    LimitType _currentLimit   = LimitType::ePrivate; // 当前行所处权限
    bool      _inCommentBlock = false;               // 当前行处于块注释中
    bool      _endAnalyseFun  = true;                // 当前函数的信息解析完毕
    bool      _endFunction    = true;                // 当前函数完结（声明、头文件中的实现）

public:
    AnalyseHeader(std::string const& contents, SettingSp setting) : _contents(contents), _setting(setting) {}

    void execute();
    void _execute(std::string const& oneLine);

    std::vector<ClassInfoSp> classes() const { return _classes; }

private:
    // 匹配类名
    static std::string analyseClassName(std::string const& line, LimitType& limit);

    // 匹配成员变量
    static VariableInfoSp analyseVariableInfo(std::string const& oneLine, LimitType limit);

    // 匹配函数名、参数、返回值及其修饰符
    static bool analyseFunction(std::string const& line, FunctionInfoSp& fun, bool& endFunction, bool& endFun);
    static void modifyFunctionReturn(std::string& funReturn, FunctionInfoSp& fun);

    // 跳过注释、空白行等
    static bool skipLine(std::string const& line, bool skipOperator, bool& commentBlock);
    // 匹配函数的权限
    static bool checkLimit(std::string const& line, LimitType& limit);

    static void inFunctionImplement(std::string const& line, bool& endFun);
};

namespace sindy
{
static bool isName(char ch)
{ // 字母、数字、下划线
    if (('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ('0' <= ch && ch <= '9') || ch == '_')
        return true;
    return false;
}

inline std::string trimFrontSpaces(std::string const& str)
{
    size_t offset = 0;
    auto   size   = str.size();
    for (auto i = 0; i < size; ++i)
    {
        if (str[i] != ' ')
            break;
        ++offset;
    }
    return str.substr(offset, size);
}
inline std::string trimBackSpaces(std::string const& str)
{
    auto   size = str.size();
    size_t end  = size;
    for (int i = size - 1; i >= 0; --i)
    {
        if (str[i] != ' ')
            break;
        --end;
    }
    if (end < size)
        return std::string(str.begin(), str.begin() + end);
    return str;
}

inline std::string removeAnnotation(std::string const& oneLine)
{
    std::vector<std::pair<int, int>> arr;

    // "fun()/* // */"   "fun()/*  */  --  /* // */"
    auto size = oneLine.size();
    for (auto i = 0; i < size;)
    {
        auto left = oneLine.find("/*", i);
        if (left == std::string::npos)
            break;

        auto right = oneLine.find("*/", left + 2);
        if (right != std::string::npos)
        {
            arr.emplace_back(std::make_pair(left, right));
            i = right + 2;
        }
        else
        {
            arr.emplace_back(std::make_pair(left, -1));
            break;
        }
    }

    std::string            line;
    std::string::size_type pos = std::string::npos;
    for (auto i = 0; i < size;)
    {
        pos = oneLine.find("//", i);
        if (pos == std::string::npos)
        {
            line = oneLine;
            break;
        }

        // "/*hello*//*world*/"
        if ((pos > 0 && oneLine[pos - 1] == '*') && (size - pos >= 3 && oneLine[pos + 2] == '*'))
        {
            i = pos + 3;
            continue;
        }

        int annotateRight = -1;
        for (auto const& [left, right] : arr)
        {
            if (left < pos && pos < right)
            {
                annotateRight = right;
                break;
            }
        }
        if (annotateRight != -1)
        {
            i = annotateRight + 2;
            continue;
        }

        size_t count = 0;
        for (auto iter = arr.rbegin(); iter != arr.rend(); ++iter)
        {
            if (iter->first < pos) // 此时不可能出现"/* // */"的情况
                break;
            ++count;
        }
        for (auto i = 0; i < count; ++i)
            arr.pop_back();

        // 裁剪
        line = oneLine.substr(0, pos);
        break;
    }

    if (line.empty())
        return line;

    pos         = 0;
    size        = arr.size();
    auto length = line.size();

    std::stringstream ss;
    for (auto i = 0; i < size; ++i)
    {
        ss << line.substr(pos, arr[i].first - pos);
        if (arr[i].second == -1)
            break;

        if (i == size - 1)
        {
            ss << line.substr(arr[i].second + 2, length - arr[i].second);
            break;
        }
        pos = arr[i].second + 2;
    }
    return size > 0 ? ss.str() : line;
}

} // namespace sindy

#endif // !ANALYSE_H
