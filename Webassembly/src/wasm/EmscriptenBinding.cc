#include "EmscriptenBinding.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <stdint.h>

// 导出全局函数
namespace sindy
{
int add(int i, int ii)
{
    return ii + i;
}
double add(double d, double dd)
{
    return d + dd;
}

// 传递字符串
emscripten::val largeText(uintptr_t buffer)
{
    char*       buf = reinterpret_cast<char*>(buffer);
    std::string str(buf);
    std::cout << ">>> wasm.cpp.largeText(), buffer = " << str.c_str() << std::endl;

    std::string result("cpp return " + str);
    return emscripten::val(result.c_str());
}

// 传递二进制流
emscripten::val binary(uintptr_t buffer, size_t size)
{
    uint8_t* buf    = nullptr;
    uint64_t length = 0;

    return emscripten::val(emscripten::typed_memory_view(length, (char*)buf));
}

Segment*                 g_pSeg = nullptr;
std::shared_ptr<Segment> g_seg  = nullptr;

// 返回指针
Segment* pointer()
{
    if (!g_pSeg)
        g_pSeg = new Segment({1, 1}, {2, 2});

    return g_pSeg;
}
std::shared_ptr<Segment> smartPointer()
{
    if (!g_seg)
        g_seg = std::make_shared<Segment>(Point{1, 1}, Point{2, 2});

    return g_seg;
}

// 返回数组
std::vector<Point> getInfoArray()
{
    return {{0, 0}, {1, 1}};
}
std::vector<Point*> getInfoArrayPtr()
{
    return {};
}

} // namespace sindy

/*
 * @brief 导出名会作为函数名的修饰，无实际用途，同一个文件中的导出名不能重复
 * @note 编译参数必须加上"--bind"，否则链接会失败
 */
EMSCRIPTEN_BINDINGS(sindy)
{
    using namespace sindy;

    // 重载函数用select_overload萃取
    // emscripten::function("add", &add); // error: no matching function for ...
    emscripten::function("add", emscripten::select_overload<int(int, int)>(&add));
    emscripten::function("add", emscripten::select_overload<double(double, double)>(&add));

    emscripten::function("largeText", &largeText);

    // 函数返回值、参数中含有指针时，必须加上allow_raw_pointers
    emscripten::function("binary", &binary, emscripten::allow_raw_pointers());

    // 导出函数返回vector
    emscripten::function("getInfoArray", &getInfoArray);
    emscripten::function("getInfoArrayPtr", &getInfoArrayPtr, emscripten::allow_raw_pointers());
    emscripten::register_vector<Segment>("vector_Segment");
    emscripten::register_vector<Segment*>("vector_Segment_ptr");

// 导出普通类
#if 1
    emscripten::class_<Point>("Point")
        .constructor<double, double>()
        .function("x", emscripten::select_overload<double() const>(&Point::x))
        .function("x", emscripten::select_overload<void(double)>(&Point::x))
        .function("y", emscripten::select_overload<double() const>(&Point::y))
        .function("y", emscripten::select_overload<void(double)>(&Point::y))
        .function("length", &Point::length)
        .class_function("distance", &Point::distance);
#else
    emscripten::class_<Point>("Point")
        .constructor<double, double>()
        .property("x", emscripten::select_overload<double() const>(&Point::x),
                  emscripten::select_overload<void(double)>(&Point::x))
        .property("y", emscripten::select_overload<double() const>(&Point::y),
                  emscripten::select_overload<void(double)>(&Point::y))
        .function("length", &Point::length)
        .class_function("distance", &Point::distance);
#endif

    // 导出子类
    emscripten::class_<Segment, emscripten::base<IGeometry>>("Segment").constructor<Point const&, Point const&>().function(
        "length", &Segment::length);

    // 返回指针
    emscripten::function("pointer", &pointer, emscripten::allow_raw_pointers());

    // 导出单例类
    emscripten::class_<SegmentManager>("SegmentManager")
        .class_function("instance", &SegmentManager::instance)
        .class_function("instancePtr", &SegmentManager::instancePtr, emscripten::allow_raw_pointers())
        .function("add", &SegmentManager::add, emscripten::allow_raw_pointers())
        .function("data", &SegmentManager::data, emscripten::allow_raw_pointers());

    // 导出模板类
}
