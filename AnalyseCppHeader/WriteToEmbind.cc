#include "WriteToEmbind.h"

#include <filesystem>

std::string WriteToEmbind::execute(std::string const& strPath)
{
    namespace fs = std::filesystem;

    fs::path path(strPath);
    if (!fs::is_directory(path))
    {
        auto name(path.filename().string());
        _execute(path.string(), name, name + "bind"); // 后缀变长
        return {};
    }

    if (_setting->_rootName.empty())
        return "error: root name is empty";

    auto rootPos = strPath.find(_setting->_rootName);
    if (rootPos == std::string::npos)
        return "error: root name not found in path";

    fs::path cur = fs::current_path();

    for (auto& el : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
    {
        if (fs::is_directory(el.path()))
            continue;

        std::string fullPath(el.path().string());
        auto        output = fullPath.substr(rootPos, fullPath.size());

        // 创建不存在的目录
        fs::path curPath(cur);
        curPath += "\\" + output + "bind"; // 后缀变长

        auto outputFull = curPath.string();

        auto newDir = outputFull.substr(0, outputFull.find_last_of('\\'));
        fs::create_directories(newDir);

        _execute(fullPath, el.path().filename().string(), outputFull);
    }
    return {};
}

void WriteToEmbind::_execute(std::string const& path, std::string const& filename, std::string const& output)
{
    if (!filename.ends_with(".h"))
        return;

    std::string str;
    sindy::readContents(path, str);

    AnalyseHeader object(str, _setting);
    object.execute();

    std::stringstream ss;
    writeTo(object, ss);

    if (auto contents(ss.str()); !contents.empty())
        sindy::writeContents(output, contents);
}

void WriteToEmbind::writeTo(AnalyseHeader const& object, std::stringstream& ss)
{
    auto const& arr = object.classes();
    for (auto const& info : arr)
    {
        if (info->_className.empty())
            continue; // error

        std::map<std::string, int> names;
        info->funcNameCount(names);

        if (info->_className == "Impl")
            auto ss = names.size(); // zh
        ss << "emscripten::class_<" << info->_className << ">(\"" << info->_className << "\")\n";

        for (auto const& fun : info->_func)
        {
            if (fun->_limit != LimitType::ePublic)
                continue;

            // 构造函数
            if (fun->_return.empty())
            {
                ss << ".constructor<" << fun->simplifyParams() << ">()\n";
                continue;
            }

            if (fun->hasFlag(FunctionInfo::eStaticFunction))
                ss << ".class_function(\""; // 静态成员函数
            else
                ss << ".function(\""; // 成员函数

            auto iter = names.find(fun->_name);
            if (iter == names.end())
                continue; // error

            if (iter->second == 1)
            {
                ss << fun->_name << "\", &" << info->_className << "::" << fun->_name << ")\n";
            }
            else
            {
                ss << fun->_name << "\", emscripten::select_overload<" << fun->simplifyFun() << ">(&" << info->_className
                   << "::" << fun->_name << "))\n";
            }
        }

        ss << ";\n\n";
    }
}