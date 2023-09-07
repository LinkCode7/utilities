#include "analyse.h"

#include <regex>

#define SPLIT_CHAR '\n'

void AnalyseHeader::execute()
{
    size_t cursor = 0;
    size_t length = _contents.size();

    while (cursor < length)
    {
        int64_t index = _contents.find(SPLIT_CHAR, cursor);
        if (index < 0)
            index = length;

        auto count = index - cursor;
        auto line  = _contents.substr(cursor, count);
        cursor     = index + 1;

        _execute(line);
    }
}

void AnalyseHeader::_execute(std::string const& oneLine)
{
    auto line = sindy::removeAnnotation(oneLine);
    line      = sindy::trimFrontSpaces(line);
    line      = sindy::trimBackSpaces(line);

    if (_info)
    {
        if (!_endFunction) // 上一个函数的申明或实现还没结束
        {
            if (auto end = line.rfind('}'); end == std::string::npos)
                return;

            _endFunction   = true;
            _endAnalyseFun = true;
        }

        // 类结束标志
        if (line.size() == 2 && line.find("};") != std::string::npos)
        {
            _classes.emplace_back(_info);
            _info = nullptr;
            return;
        }

        inFunctionImplement(line, _endFunction);

        if (skipLine(line, _setting->_skipOperator, _inCommentBlock))
        {
            if (auto variable = analyseVariableInfo(line, _currentLimit); variable)
            {
                _info->_member.emplace_back(variable);
                return;
            }

            checkLimit(line, _currentLimit);
            return;
        }
        if (_inCommentBlock)
            return;

        // 删除单行内的注释："void fun(int a/*...*/);"   "void fun(); // ..."

        FunctionInfoSp fun = _endAnalyseFun ? std::make_shared<FunctionInfo>(_currentLimit) : _info->_func.back();
        if (analyseFunction(line, fun, _endAnalyseFun, _endFunction))
            _info->_func.emplace_back(fun);
    }
    else
    {
        if (auto name = analyseClassName(line, _currentLimit); !name.empty())
        {
            _info             = std::make_shared<ClassInfo>();
            _info->_className = name;
        }
    }
}

std::string AnalyseHeader::analyseClassName(std::string const& line, LimitType& limit)
{
    // "class Arc2d : public Geometry" "class GTEST_API_ AssertionResult"
    if (line.find("enum") != std::string::npos)
        return {};

#if 0
    std::regex class_regex(R"(\bclass\s+(\w+)\b)"); // \b单词边界,\w等价于"[A-Za-z0-9_]"

    std::smatch match;
    if (std::regex_search(line, match, class_regex))
        return match[1].str();
#else
    size_t keyLength = 5;
    auto   pos       = line.find("class");
    if (pos == std::string::npos)
    {
        keyLength = 6;
        pos       = line.find("struct");
        limit     = LimitType::ePublic;
    }

    if (pos == std::string::npos)
        return {};

    if (pos > 1 && sindy::isName(line[pos - 1])) // 必须是单词边界
        return {};

    // 类的申明:"class Object"
    if (line.rfind(';') != std::string::npos)
        return {};

    // 模板"template <class Impl>"
    if (auto key = line.find("template"); key != std::string::npos)
    {
        auto left = line.find("<");
        if (left != std::string::npos && left > key)
        {
            if (auto right = line.find('>'); right != std::string::npos)
            {
                if (right > left && right > pos + keyLength)
                    return {};
            }
        }
    }

    pos += keyLength;

    if (line[pos] != ' ') // 必须是单词边界
        return {};

    auto   size  = line.size();
    size_t begin = line.find_first_not_of(" ", pos);

    std::string firstName;
    auto        i = begin;
    for (; i < size; ++i)
    {
        if (line[i] == ':')
        {
            firstName = line.substr(begin, i - begin);
            return {};
        }
        if (line[i] == ' ')
        {
            firstName = line.substr(begin, i - begin);
            break;
        }
    }
    if (firstName.empty() && i > begin)
    {
        firstName = line.substr(begin, i - begin);
        return firstName;
    }

    // real name
    begin = i + 1;
    std::string secondName;
    for (i = begin; i < size; ++i)
    {
        if (line[i] == ' ' || line[i] == ':')
        {
            secondName = line.substr(begin, i - begin);
            break;
        }
    }
    if (secondName.empty() && i > begin)
    {
        secondName = line.substr(begin, i - begin);
        if (!secondName.empty())
            return secondName;
    }

    return secondName.empty() ? firstName : secondName;
#endif

    return {};
}

// 假定一行只有一个函数
bool AnalyseHeader::analyseFunction(std::string const& line, FunctionInfoSp& fun, bool& endAnalyse, bool& endFun)
{
    if (!fun)
        return false;

    auto symbol = line.rfind('=');
    if (symbol != std::string::npos)
    {
        if (auto theDelete = line.find("delete", symbol); theDelete != std::string::npos)
            return false;
        if (auto theDefault = line.find("default", symbol); theDefault != std::string::npos)
            return false;
    }

    auto left = line.find('(');
    if (left == std::string::npos)
        return false;

    // 函数名
    int  index = 0;
    auto size  = line.size();
    for (int i = left - 1; i >= 0; --i)
    {
        if (sindy::isName(line[i]))
        {
            ++index;
            continue;
        }
        break;
    }

    int funBegin = left - index;
    fun->_name   = line.substr(funBegin, index);

    // 返回值
    fun->_return = line.substr(0, funBegin);
    modifyFunctionReturn(fun->_return, fun);

    auto right = line.find(')');
    if (right != std::string::npos)
    {
        endAnalyse = true;
        // 参数
        fun->_params = line.substr(left + 1, right - left - 1);

        // 函数const
        if (auto theConst = line.find("const", right); theConst != std::string::npos)
            fun->addFlag(FunctionInfo::eConstFunction);
    }
    else
    {
        endAnalyse = false;
    }

    /*
     * 1: void fun();
     * 2: void fun() { }
     * 3: void fun()
     *    {
     *        // ...
     *    }
     */
    if (auto end = line.rfind(';'); end != std::string::npos)
    {
        endFun = true; // 1
    }
    else
    {
        if (end = line.rfind('}'); end == std::string::npos)
            endFun = false; // 3
        else
            endFun = true; // 2
    }

    return true;
}

void AnalyseHeader::modifyFunctionReturn(std::string& funReturn, FunctionInfoSp& fun)
{
    auto pos = funReturn.find("static");
    if (pos != std::string::npos)
    {
        funReturn = funReturn.replace(pos, 6, "");
        fun->addFlag(FunctionInfo::eStaticFunction);
    }

    pos = funReturn.find("inline");
    if (pos != std::string::npos)
    {
        funReturn = funReturn.replace(pos, 6, "");
        fun->addFlag(FunctionInfo::eInlineFunction);
    }

    pos = funReturn.find("virtual");
    if (pos != std::string::npos)
    {
        funReturn = funReturn.replace(pos, 7, "");
        fun->addFlag(FunctionInfo::eVirtualFunction);
    }

    pos = funReturn.find("override");
    if (pos != std::string::npos)
    {
        funReturn = funReturn.replace(pos, 8, "");
        fun->addFlag(FunctionInfo::eOverrideFunction);
    }

    pos = funReturn.find("explicit");
    if (pos != std::string::npos)
    {
        funReturn = funReturn.replace(pos, 8, "");
        fun->addFlag(FunctionInfo::eExplicitFunction);
    }

    funReturn = sindy::trimFrontSpaces(funReturn);
    funReturn = sindy::trimBackSpaces(funReturn);
}

bool AnalyseHeader::skipLine(std::string const& line, bool skipOperator, bool& inCommentBlock)
{
    auto size = line.size();
    if (size <= 2)
        return true;

    // 跳过非函数申明的行
    if (line.rfind("(") == std::string::npos)
        return true;

    // 跳过注释
    if (line[0] == '/')
    {
        if (line[1] == '/')
            return true;
        if (line[1] == '*')
        {
            if (size >= 4 && line[size - 2] != '*' && line[size - 1] != '/')
                inCommentBlock = true;
            return true;
        }
    }

    if (inCommentBlock)
    {
        if (line[size - 2] == '*' && line[size - 1] == '/') // 块注释结束
            inCommentBlock = false;
    }

    if (skipOperator && line.find("operator") != std::string::npos)
        return true;

    return false;
}

bool AnalyseHeader::checkLimit(std::string const& line, LimitType& limit)
{
    if (line.rfind(':') == std::string::npos)
        return false;

    auto str = sindy::trimFrontSpaces(line);

    if (str.find("public") == 0)
    {
        limit = LimitType::ePublic;
        return true;
    }
    if (str.find("protected") == 0)
    {
        limit = LimitType::eProtected;
        return true;
    }
    if (str.find("private") == 0)
    {
        limit = LimitType::ePrivate;
        return true;
    }
    return false;
}

void AnalyseHeader::inFunctionImplement(std::string const& line, bool& endFun)
{
    auto left = line.rfind('{');
    if (left == std::string::npos)
        return;

    if (line.find('(') != std::string::npos || line.find("const") != std::string::npos ||
        line.find("bool") != std::string::npos || line.find("int") != std::string::npos ||
        line.find("double") != std::string::npos || line.find("operator") != std::string::npos)
    {
        // maybeFun = true;
    }
    else
    {
        if (line.size() == 1)
            return; // 类的第二行
    }

    if (auto right = line.rfind('}'); right != std::string::npos)
    {
        if (right > left)
            return;
    }
    endFun = false;
}

// 假定变量不换行，假定已删除行尾空格
VariableInfoSp AnalyseHeader::analyseVariableInfo(std::string const& oneLine, LimitType limit)
{
    if (oneLine.find('(') != std::string::npos)
        return nullptr;

    auto size = oneLine.size();
    if (size == 0)
        return nullptr;

    if (oneLine[size - 1] != ';')
        return nullptr;

    std::string line(oneLine);

    auto variable = std::make_shared<VariableInfo>(limit);
    auto pos      = line.find("static");
    if (pos != std::string::npos)
    {
        line = line.replace(pos, 6, "");
        variable->addFlag(VariableInfo::eStaticVariable);
    }

    pos = line.find("const");
    if (pos != std::string::npos)
    {
        line = line.replace(pos, 5, "");
        variable->addFlag(VariableInfo::eConstVariable);
    }

    // 开始分割

    int begin = 0;
    // "std::map<int, int> _xxx;"
    if (line.find('<') != std::string::npos)
    {
        if (auto r = line.rfind('>'); r != std::string::npos)
            begin = r + 1;
    }

    line = sindy::trimFrontSpaces(line);
    pos  = oneLine.find(' ', begin);
    if (pos == std::string::npos)
        return nullptr;

    variable->_type = line.substr(0, pos);

    std::string name = line.substr(pos + 1, (size - 1) - (pos + 1));
    if (auto assign = name.find('='); assign != std::string::npos)
    {
        variable->_name    = name.substr(0, assign - 1);
        variable->_default = name.substr(assign + 1, (size - 2) - (assign + 1));
    }
    else
    {
        variable->_name = name;
    }

    variable->_name    = sindy::trimFrontSpaces(variable->_name);
    variable->_name    = sindy::trimBackSpaces(variable->_name);
    variable->_default = sindy::trimFrontSpaces(variable->_default);
    variable->_default = sindy::trimBackSpaces(variable->_default);
    return variable;
}

void ClassInfo::funcNameCount(std::map<std::string, int>& names) const
{
    for (auto const& fun : _func)
    {
        auto iter = names.find(fun->_name);
        if (iter == names.end())
            names[fun->_name] = 1;
        else
            iter->second++;
    }
}

std::string FunctionInfo::simplifyParams() const
{
    int size = _params.size();

    bool firstNotFind = true;

    int begin = 0;
    int end   = _params.find(',');
    if (end == std::string::npos)
    {
        end == size;
        firstNotFind = false;
    }

    std::stringstream ss;
    while (end <= size)
    {
        auto param = _params.substr(begin, end - begin);
        param      = sindy::trimFrontSpaces(param);
        param      = sindy::trimBackSpaces(param);
        int len    = param.size();

        int count = 0;
        for (int i = len - 1; i >= 0; --i)
        {
            if (sindy::isName(param[i]))
            {
                ++count;
                continue;
            }
            break;
        }

        param = param.substr(0, len - count);
        param = sindy::trimBackSpaces(param);
        ss << param;
        if (firstNotFind && end < size)
            ss << ", ";

        begin = end + 1;
        end   = _params.find(',', end + 1);

        if (end == std::string::npos)
        {
            if (firstNotFind)
            {
                end          = size;
                firstNotFind = false;
            }
            else
            {
                break;
            }
        }
    }
    return ss.str();
}

std::string FunctionInfo::simplifyFun() const
{
    std::stringstream ss;
    if (hasFlag(eConstFunction))
        ss << _return << '(' << simplifyParams() << ')' << "const";
    else
        ss << _return << '(' << simplifyParams() << ')';
    return ss.str();
}