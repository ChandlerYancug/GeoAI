/* FileName: test_geometry.cpp
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 几何类完整单元测试
*/

#include "Geometry.h"
#include "GeometryAlgorithm.h"
#include <cassert>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main() {
    std::cout << "========== 几何类单元测试 ==========" << std::endl;

    // 测试 Point
    Point oPoint1(0, 0);
    Point oPoint2(3, 4);
    assert(oPoint1.distanceTo(oPoint2) == 5.0);
    std::cout << "测试 Point... 通过" << std::endl;

    // 测试 LineString
    std::vector<Point> vPoints = { Point(0,0), Point(10,0), Point(10,10) };
    LineString oLine(vPoints);
    assert(oLine.getPointCount() == 3);
    assert(std::abs(oLine.getLength() - 20.0) < 1e-9);
    std::cout << "测试 LineString... 通过" << std::endl;

    // 测试 Polygon
    std::vector<Point> vSquare = { Point(0,0), Point(10,0), Point(10,10), Point(0,10), Point(0,0) };
    LineString oRing(vSquare);
    Polygon oPoly(oRing);
    assert(std::abs(oPoly.getArea() - 100.0) < 1e-9);
    assert(oPoly.containsPoint(Point(5, 5)));
    assert(!oPoly.containsPoint(Point(15, 15)));
    std::cout << "测试 Polygon... 通过" << std::endl;

    // 测试 Envelope
    Envelope oEnv = Envelope::fromPoints(vPoints);
    assert(oEnv.contains(Point(5, 5)));
    assert(!oEnv.contains(Point(15, 15)));
    std::cout << "测试 Envelope... 通过" << std::endl;

    // 测试 Buffer 算法
    Polygon oBuffer = GeometryAlgorithm::pointBuffer(Point(0, 0), 10.0, 36);
    assert(std::abs(oBuffer.getArea() - M_PI * 100.0) < 1.0);
    std::cout << "测试 Buffer 算法... 通过" << std::endl;

    // 测试距离算法
    std::vector<Point> vLine = { Point(0,0), Point(10,0) };
    LineString oTestLine(vLine);
    double dDist = GeometryAlgorithm::pointToLineStringDistance(Point(5, 5), oTestLine);
    assert(std::abs(dDist - 5.0) < 1e-9);
    std::cout << "测试距离算法... 通过" << std::endl;

    std::cout << "========== 所有测试通过！ ==========" << std::endl;
    std::cout << std::endl;
    std::cout << "【统一接口】已提供：" << std::endl;
    std::cout << "  - Geometry::toWKT() / fromWKT() : 供模块二数据库使用" << std::endl;
    std::cout << "  - GeometryAlgorithm::pointBuffer() : 供模块三缓冲区分析" << std::endl;
    std::cout << "  - GeometryAlgorithm::polygonBuffer() : 供模块三缓冲区分析" << std::endl;
    std::cout << "  - GeometryAlgorithm::intersectPolygons() : 供模块三叠加分析" << std::endl;
    std::cout << "  - GeometryAlgorithm::unionPolygons() : 供模块三叠加分析" << std::endl;

    return 0;
}