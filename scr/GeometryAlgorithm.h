/* FileName: GeometryAlgorithm.h
   Author: YourName
   Version: 1.0
   Date: 2026-01-15
   Description: 几何算法（缓冲区、交并差、距离），供模块三队友调用
*/

#ifndef GEOMETRY_ALGORITHM_H
#define GEOMETRY_ALGORITHM_H

#include "Geometry.h"
#include <vector>
#include <memory>

/* 几何算法静态类 */
class GeometryAlgorithm {
public:
    /* 计算点到线串的最短距离 */
    static double pointToLineStringDistance(const Point& oPoint, const LineString& oLine);

    /* 计算点到多边形的最短距离（负值表示内部） */
    static double pointToPolygonDistance(const Point& oPoint, const Polygon& oPolygon);

    /* 计算点缓冲区的近似多边形（圆形近似为n边正多边形） */
    static Polygon pointBuffer(const Point& oPoint, double dRadius, int nSegments = 36);

    /* 【统一接口】计算多边形缓冲区（供模块三队友调用） */
    static Polygon polygonBuffer(const Polygon& oPolygon, double dRadius, int nSegments = 36);

    /* 【统一接口】计算两个多边形的交集（供模块三队友调用） */
    static std::vector<Polygon> intersectPolygons(const Polygon& oPolyA, const Polygon& oPolyB);

    /* 【统一接口】计算两个多边形的并集（供模块三队友调用） */
    static std::vector<Polygon> unionPolygons(const Polygon& oPolyA, const Polygon& oPolyB);

    /* 计算多边形的最小包络矩形 */
    static Polygon toEnvelopePolygon(const Polygon& oPolygon);
};

#endif