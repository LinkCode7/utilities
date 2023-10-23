#ifndef EMSCRIPTEN_BINDING_H
#define EMSCRIPTEN_BINDING_H
#include <iostream>
#include <vector>

namespace sindy
{
class Point
{
    double _x;
    double _y;

public:
    Point(double xx, double yy) : _x(xx), _y(yy)
    { // Js.Module.Point()会执行构造
        std::cout << ">>> Point::Point(double xx, double yy)" << std::endl;
    }
    ~Point()
    { // Js.EmbindObject.delete()会执行析构
        std::cout << ">>> Point::~Point()" << std::endl;
    }

    inline double x() const { return _x; }
    inline void   x(double value) { _x = value; }

    inline double y() const { return _y; }
    inline void   y(double value) { _y = value; }

    inline Point  operator-(const Point& pt) const { return Point(_x - pt._x, _y - pt._y); }
    inline double length() const { return sqrt(_x * _x + _y * _y); };

    static double distance(Point const& pt1, Point const& pt2) { return (pt1 - pt2).length(); }
};

// inheritance system
class IGeometry
{
public:
    virtual Point begin() const = 0;

    double distance() const { return 0.0; }

    static double getDistance(IGeometry*) { return 0.0; }
};

class Segment : public IGeometry
{
    Point _begin;
    Point _end;

public:
    Segment(Point const& begin, Point const& end) : _begin(begin), _end(end) {}

    Point begin() const override { return _begin; }

    inline double length() const { return (_begin - _end).length(); }
};

class SegmentManager
{
    std::vector<Segment*> _segments;
    SegmentManager() {}

public:
    static SegmentManager& instance()
    {
        static SegmentManager opr;
        return opr;
    }
    static SegmentManager* instancePtr() { return &instance(); }

    void                  add(Segment* object) { _segments.emplace_back(object); }
    std::vector<Segment*> data() const { return _segments; }
};

} // namespace sindy

#endif // !EMSCRIPTEN_BINDING_H
