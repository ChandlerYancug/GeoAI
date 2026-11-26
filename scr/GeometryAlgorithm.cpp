/* FileName: GeometryAlgorithm.cpp
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 几何算法实现
*/

#include "GeometryAlgorithm.h"
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double GeometryAlgorithm::pointToLineStringDistance(const Point& oPoint, const LineString& oLine) {
    const auto& vPts = oLine.vPoints;
    int nCount = static_cast<int>(vPts.size());
    if (nCount < 2) return std::numeric_limits<double>::max();

    double dMinDist = std::numeric_limits<double>::max();

    for (int nIdx = 0; nIdx < nCount - 1; ++nIdx) {
        const Point& oA = vPts[nIdx];
        const Point& oB = vPts[nIdx + 1];

        double dAx = oPoint.dX - oA.dX;
        double dAy = oPoint.dY - oA.dY;
        double dBx = oB.dX - oA.dX;
        double dBy = oB.dY - oA.dY;
        double dDot = dAx * dBx + dAy * dBy;
        double dLen2 = dBx * dBx + dBy * dBy;

        double dT = (dLen2 > 1e-9) ? dDot / dLen2 : 0.0;
        dT = std::max(0.0, std::min(1.0, dT));

        double dProjX = oA.dX + dT * dBx;
        double dProjY = oA.dY + dT * dBy;

        double dDx = oPoint.dX - dProjX;
        double dDy = oPoint.dY - dProjY;
        double dDist = std::sqrt(dDx * dDx + dDy * dDy);

        dMinDist = std::min(dMinDist, dDist);
    }
    return dMinDist;
}

double GeometryAlgorithm::pointToPolygonDistance(const Point& oPoint, const Polygon& oPolygon) {
    if (oPolygon.containsPoint(oPoint)) {
        return -1.0;
    }
    return pointToLineStringDistance(oPoint, oPolygon.getOuterRing());
}

Polygon GeometryAlgorithm::pointBuffer(const Point& oPoint, double dRadius, int nSegments) {
    std::vector<Point> vPoints;
    for (int nIdx = 0; nIdx <= nSegments; ++nIdx) {
        double dAngle = 2.0 * M_PI * nIdx / nSegments;
        double dX = oPoint.dX + dRadius * std::cos(dAngle);
        double dY = oPoint.dY + dRadius * std::sin(dAngle);
        vPoints.emplace_back(dX, dY);
    }
    LineString oRing(vPoints);
    return Polygon(oRing);
}

Polygon GeometryAlgorithm::polygonBuffer(const Polygon& oPolygon, double dRadius, int nSegments) {
    Envelope oEnv = oPolygon.getEnvelope();
    Point oCenter = oEnv.getCenter();
    double dDiag = std::sqrt(oEnv.getWidth() * oEnv.getWidth() + oEnv.getHeight() * oEnv.getHeight());
    double dBufferRadius = dRadius + dDiag / 2.0;
    return pointBuffer(oCenter, dBufferRadius, nSegments);
}

std::vector<Polygon> GeometryAlgorithm::intersectPolygons(const Polygon& oPolyA, const Polygon& oPolyB) {
    std::vector<Polygon> vResult;
    Envelope oEnvA = oPolyA.getEnvelope();
    Envelope oEnvB = oPolyB.getEnvelope();
    if (!oEnvA.intersects(oEnvB)) {
        return vResult;
    }
    vResult.push_back(GeometryAlgorithm::toEnvelopePolygon(oPolyA));
    return vResult;
}

std::vector<Polygon> GeometryAlgorithm::unionPolygons(const Polygon& oPolyA, const Polygon& oPolyB) {
    std::vector<Polygon> vResult;
    vResult.push_back(oPolyA);
    vResult.push_back(oPolyB);
    return vResult;
}

Polygon GeometryAlgorithm::toEnvelopePolygon(const Polygon& oPolygon) {
    Envelope oEnv = oPolygon.getEnvelope();
    std::vector<Point> vPoints = {
        Point(oEnv.dMinX, oEnv.dMinY),
        Point(oEnv.dMaxX, oEnv.dMinY),
        Point(oEnv.dMaxX, oEnv.dMaxY),
        Point(oEnv.dMinX, oEnv.dMaxY),
        Point(oEnv.dMinX, oEnv.dMinY)
    };
    LineString oRing(vPoints);
    return Polygon(oRing);
}