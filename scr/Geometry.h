/* FileName: Geometry.h
   Author: YourName
   Version: 1.0
   Date: 2026-01-15
   Description: 自建几何类型（点/线/面/包络矩形），零依赖，供模块一及队友使用
*/

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

/* 几何类型枚举 */
enum class GeometryType {
    POINT,
    LINE_STRING,
    POLYGON,
    ENVELOPE
};

/* 二维点 */
class Point {
public:
    double dX;
    double dY;

    Point(double dXVal = 0.0, double dYVal = 0.0);

    /* 计算与另一点的距离 */
    double distanceTo(const Point& oOther) const;

    /* 判断是否相等 */
    bool equals(const Point& oOther, double dTolerance = 1e-9) const;

    /* 转换为字符串（用于调试） */
    std::string toString() const;
};

/* 线串（由有序点构成） */
class LineString {
public:
    std::vector<Point> vPoints;

    LineString() = default;
    explicit LineString(const std::vector<Point>& vPointsVal);

    /* 获取点数 */
    int getPointCount() const;

    /* 计算线串长度 */
    double getLength() const;

    /* 获取包络矩形 */
    class Envelope getEnvelope() const;

    /* 判断是否闭合（首尾点相同） */
    bool isClosed() const;
};

/* 多边形（由外环和若干内环构成） */
class Polygon {
public:
    std::vector<LineString> vRings;  // vRings[0]为外环，后续为内环（洞）

    Polygon() = default;
    explicit Polygon(const LineString& oOuterRing);

    /* 获取外环 */
    LineString getOuterRing() const;

    /* 获取内环数量 */
    int getHoleCount() const;

    /* 获取指定内环 */
    LineString getHole(int nIndex) const;

    /* 添加内环 */
    void addHole(const LineString& oHole);

    /* 计算多边形面积（有符号面积，正数为逆时针） */
    double getArea() const;

    /* 判断多边形是否包含点 */
    bool containsPoint(const Point& oPoint) const;

    /* 获取包络矩形 */
    class Envelope getEnvelope() const;

    /* 判断多边形是否有效（不自交，外环逆时针/内环顺时针） */
    bool isValid() const;
};

/* 包络矩形（轴对齐） */
class Envelope {
public:
    double dMinX;
    double dMinY;
    double dMaxX;
    double dMaxY;

    Envelope();
    Envelope(double dMinXVal, double dMinYVal, double dMaxXVal, double dMaxYVal);

    /* 从点集构造 */
    static Envelope fromPoints(const std::vector<Point>& vPoints);

    /* 合并另一个包络矩形 */
    void merge(const Envelope& oOther);

    /* 判断是否包含点 */
    bool contains(const Point& oPoint) const;

    /* 判断是否与另一个包络矩形相交 */
    bool intersects(const Envelope& oOther) const;

    /* 获取宽度 */
    double getWidth() const;

    /* 获取高度 */
    double getHeight() const;

    /* 获取中心点 */
    Point getCenter() const;
};

/* 通用几何体（联合类型，方便多态使用） */
class Geometry {
public:
    GeometryType eType;

    /* 联合体存储具体几何数据 */
    union {
        Point oPoint;
        LineString oLineString;
        Polygon oPolygon;
        Envelope oEnvelope;
    };

    /* 构造函数 */
    Geometry();
    explicit Geometry(const Point& oPointVal);
    explicit Geometry(const LineString& oLineStringVal);
    explicit Geometry(const Polygon& oPolygonVal);
    explicit Geometry(const Envelope& oEnvelopeVal);

    /* 拷贝构造 */
    Geometry(const Geometry& oOther);

    /* 赋值运算符 */
    Geometry& operator=(const Geometry& oOther);

    /* 析构函数 */
    ~Geometry();

    /* 获取包络矩形 */
    Envelope getEnvelope() const;

    /* 【统一接口】转换为WKT字符串（供队友的数据库模块使用） */
    std::string toWKT() const;

    /* 【统一接口】从WKT字符串解析（供队友的数据库模块使用） */
    static Geometry fromWKT(const std::string& sWKT);
};

#endif