#ifndef SETTING_H
#define SETTING_H

#include <set>

#include "./iguana/iguana/json_reader.hpp"
#include "./iguana/iguana/json_writer.hpp"

class Setting
{
public:
    std::string              _directory;           // 最后一次选择的目录或文件名
    bool                     _skipOperator = true; // 忽略操作符重载
    std::string              _rootName;            // 根目录的名称
    std::vector<std::string> _skipDir;             // 忽略的目录名

    bool _modify = false; // 运行时变量

    REFLECTION_ALIAS(Setting, "Setting", FLDALIAS(&Setting::_directory, "directory"),
                     FLDALIAS(&Setting::_skipOperator, "skipOperator"), FLDALIAS(&Setting::_rootName, "rootName"),
                     FLDALIAS(&Setting::_skipDir, "skipDir"));
};

using SettingSp = std::shared_ptr<Setting>;

#endif // !SETTING_H
