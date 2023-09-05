#ifndef WRITE_TO_EMBIND_H
#define WRITE_TO_EMBIND_H

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "analyse.h"

class WriteToEmbind
{
    SettingSp _setting;

public:
    WriteToEmbind(SettingSp setting) : _setting(setting) {}
    std::string execute(std::string const& strPath);

private:
    void _execute(std::string const& path, std::string const& filename, std::string const& output);

    void writeTo(AnalyseHeader const& object, std::stringstream& ss);
};

namespace sindy
{
inline bool readContents(std::string const& filename, std::string& text)
{
    std::fstream fs;
    fs.open(filename, std::ios::in);
    if (!fs.is_open())
        return false;

    std::stringstream ss;
    ss << fs.rdbuf();
    fs.close();

    text = ss.str();
    return true;
}
inline void writeContents(std::string const& filename, std::string const& text)
{
    std::fstream fs;
    fs.open(filename, std::ios::out);
    fs.write(text.c_str(), text.size());
    fs.close();
}
} // namespace sindy

#endif // !WRITE_TO_EMBIND_H
