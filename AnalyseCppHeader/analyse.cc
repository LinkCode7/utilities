#include "analyse.h"

#include <regex>

#define SPLIT_CHAR '\n'
#define TEST_DATA_DIR "../unittest/data/"

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

    if (_info)
    {
        if (!_endFunction) // 上一个函数的申明或实现还没结束
        {
            if (auto end = line.rfind('}'); end == std::string::npos)
                return;
            else
                _endFunction = true;
        }

        // 类结束标志
        if (line.size() == 2 && line.find("};") != std::string::npos)
        {
            _classes.emplace_back(_info);
            _info = nullptr;
            return;
        }

        if (skipLine(line, _setting->_skipOperator, _inCommentBlock))
        {
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
        if (auto name = analyseClassName(line); !name.empty())
        {
            _info             = std::make_shared<ClassInfo>();
            _info->_className = name;
        }
    }
}

std::string AnalyseHeader::analyseClassName(std::string const& line)
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
    }

    if (pos == std::string::npos)
        return {};

    if (pos > 1 && sindy::isName(line[pos - 1])) // 必须是单词边界
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

    int funBegin  = left - index;
    fun->_funName = line.substr(funBegin, index);

    // 返回值
    fun->_funReturn = line.substr(0, funBegin);
    modifyFunctionReturn(fun->_funReturn, fun);

    auto right = line.find(')');
    if (right != std::string::npos)
    {
        endAnalyse = true;
        // 参数
        fun->_funParams = line.substr(left + 1, right - left - 1);

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
    if (line.rfind(";") == std::string::npos && line[size - 1] != ')')
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

bool AnalyseHeader::checkLimit(std::string const& line, FuncLimit& limit)
{
    if (line.rfind(':') == std::string::npos)
        return false;

    auto str = sindy::trimFrontSpaces(line);

    if (str.find("public") == 0)
    {
        limit = FuncLimit::ePublic;
        return true;
    }
    if (str.find("protected") == 0)
    {
        limit = FuncLimit::eProtected;
        return true;
    }
    if (str.find("private") == 0)
    {
        limit = FuncLimit::ePrivate;
        return true;
    }
    return false;
}

void ClassInfo::funcNameCount(std::map<std::string, int>& names)
{
    for (auto const& fun : _func)
    {
        auto iter = names.find(fun->_funName);
        if (iter == names.end())
            names[fun->_funName] = 1;
        else
            iter->second++;
    }
}

std::string FunctionInfo::simplifyParams() const
{
    int size = _funParams.size();

    bool firstNotFind = true;

    int begin = 0;
    int end   = _funParams.find(',');
    if (end == std::string::npos)
    {
        end == size;
        firstNotFind = false;
    }

    std::stringstream ss;
    while (end <= size)
    {
        auto param = _funParams.substr(begin, end - begin);
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
        end   = _funParams.find(',', end + 1);

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
        ss << _funReturn << '(' << simplifyParams() << ')' << "const";
    else
        ss << _funReturn << '(' << simplifyParams() << ')';
    return ss.str();
}