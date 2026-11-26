/* FileName: Geometry.cpp
   Author: YourName
   Version: 1.0
   Date: 2026-01-15
   Description: 几何类型实现
*/

#include "Geometry.h"
#include <sstream>
#include <iomanip>

/* ========== Point 实现 ========== */

Point::Point(double dXVal, double dYVal) : dX(dXVal), dY(dYVal) {}

double Point::distanceTo(const Point& oOther) const {
    double dDx = dX - oOther.dX;
    double dDy = dY - oOther.dY;
    return std::sqrt(dDx * dDx + dDy * dDy);
}

bool Point::equals(const Point& oOther, double dTolerance) const {
    return std::abs(dX - oOther.dX) < dTolerance &&
        std::abs(dY - oOther.dY) < dTolerance;
}

std::string Point::toString() const {
    std::ostringstream oStream;
    oStream << "POINT(" << dX << " " << dY << ")";
    return oStream.str();
}

/* ========== LineString 实现 ========== */

LineString::LineString(const std::vector<Point>& vPointsVal) : vPoints(vPointsVal) {}

int LineString::getPointCount() const {
    return static_cast<int>(vPoints.size());
}

double LineString::getLength() const {
    double dTotalLength = 0.0;
    int nPointCount = getPointCount();
    for (int nIdx = 0; nIdx < nPointCount - 1; ++nIdx) {
        dTotalLength += vPoints[nIdx].distanceTo(vPoints[nIdx + 1]);
    }
    return dTotalLength;
}

Envelope LineString::getEnvelope() const {
    return Envelope::fromPoints(vPoints);
}

bool LineString::isClosed() const {
    if (vPoints.size() < 3) return false;
    return vPoints.front().equals(vPoints.back());
}

/* ========== Polygon 实现 ========== */

Polygon::Polygon(const LineString& oOuterRing) {
    if (oOuterRing.isClosed()) {
        vRings.push_back(oOuterRing);
    }
}

LineString Polygon::getOuterRing() const {
    if (vRings.empty()) return LineString();
    return vRings[0];
}

int Polygon::getHoleCount() const {
    if (vRings.empty()) return 0;
    return static_cast<int>(vRings.size()) - 1;
}

LineString Polygon::getHole(int nIndex) const {
    if (nIndex < 0 || nIndex >= getHoleCount()) return LineString();
    return vRings[nIndex + 1];
}

void Polygon::addHole(const LineString& oHole) {
    if (oHole.isClosed()) {
        vRings.push_back(oHole);
    }
}

double Polygon::getArea() const {
    if (vRings.empty()) return 0.0;

    /* 计算有符号面积（鞋带公式） */
    auto computeRingArea = [](const LineString& oRing) -> double {
        const auto& vPts = oRing.vPoints;
        int nSize = static_cast<int>(vPts.size());
        if (nSize < 3) return 0.0;

        double dArea = 0.0;
        for (int nIdx = 0; nIdx < nSize; ++nIdx) {
            int nNextIdx = (nIdx + 1) % nSize;
            dArea += vPts[nIdx].dX * vPts[nNextIdx].dY;
            dArea -= vPts[nNextIdx].dX * vPts[nIdx].dY;
        }
        return dArea / 2.0;
        };

    double dArea = computeRingArea(vRings[0]);
    int nHoleCount = getHoleCount();
    for (int nHoleIdx = 0; nHoleIdx < nHoleCount; ++nHoleIdx) {
        dArea -= computeRingArea(vRings[nHoleIdx + 1]);
    }
    return std::abs(dArea);
}

bool Polygon::containsPoint(const Point& oPoint) const {
    if (vRings.empty()) return false;

    /* 射线法判断点是否在多边形内 */
    auto pointInRing = [](const Point& oPt, const LineString& oRing) -> bool {
        const auto& vPts = oRing.vPoints;
        int nSize = static_cast<int>(vPts.size());
        if (nSize < 3) return false;

        bool bInside = false;
        for (int nIdx = 0, nJdx = nSize - 1; nIdx < nSize; nJdx = nIdx++) {
            const Point& oP1 = vPts[nIdx];
            const Point& oP2 = vPts[nJdx];

            bool bIntersect = ((oP1.dY > oPt.dY) != (oP2.dY > oPt.dY)) &&
                (oPt.dX < (oP2.dX - oP1.dX) * (oPt.dY - oP1.dY) / (oP2.dY - oP1.dY) + oP1.dX);
            if (bIntersect) {
                bInside = !bInside;
            }
        }
        return bInside;
        };

    if (!pointInRing(oPoint, vRings[0])) return false;

    int nHoleCount = getHoleCount();
    for (int nHoleIdx = 0; nHoleIdx < nHoleCount; ++nHoleIdx) {
        if (pointInRing(oPoint, vRings[nHoleIdx + 1])) return false;
    }
    return true;
}

Envelope Polygon::getEnvelope() const {
    if (vRings.empty()) return Envelope();
    return vRings[0].getEnvelope();
}

bool Polygon::isValid() const {
    /* 简化验证：至少要有外环 */
    if (vRings.empty()) return false;

    /* 外环面积应为正（逆时针） */
    double dOuterArea = 0.0;
    const auto& vOuterPts = vRings[0].vPoints;
    int nOuterSize = static_cast<int>(vOuterPts.size());
    for (int nIdx = 0; nIdx < nOuterSize; ++nIdx) {
        int nNextIdx = (nIdx + 1) % nOuterSize;
        dOuterArea += vOuterPts[nIdx].dX * vOuterPts[nNextIdx].dY;
        dOuterArea -= vOuterPts[nNextIdx].dX * vOuterPts[nIdx].dY;
    }

    /* 外环应为逆时针（面积>0），简单接受两种方向 */
    return nOuterSize >= 3;
}

/* ========== Envelope 实现 ========== */

Envelope::Envelope()
    : dMinX(1e9), dMinY(1e9), dMaxX(-1e9), dMaxY(-1e9) {
}

Envelope::Envelope(double dMinXVal, double dMinYVal, double dMaxXVal, double dMaxYVal)
    : dMinX(dMinXVal), dMinY(dMinYVal), dMaxX(dMaxXVal), dMaxY(dMaxYVal) {
}

Envelope Envelope::fromPoints(const std::vector<Point>& vPoints) {
    Envelope oEnv;
    for (const auto& oPoint : vPoints) {
        oEnv.dMinX = std::min(oEnv.dMinX, oPoint.dX);
        oEnv.dMinY = std::min(oEnv.dMinY, oPoint.dY);
        oEnv.dMaxX = std::max(oEnv.dMaxX, oPoint.dX);
        oEnv.dMaxY = std::max(oEnv.dMaxY, oPoint.dY);
    }
    return oEnv;
}

void Envelope::merge(const Envelope& oOther) {
    dMinX = std::min(dMinX, oOther.dMinX);
    dMinY = std::min(dMinY, oOther.dMinY);
    dMaxX = std::max(dMaxX, oOther.dMaxX);
    dMaxY = std::max(dMaxY, oOther.dMaxY);
}

bool Envelope::contains(const Point& oPoint) const {
    return oPoint.dX >= dMinX && oPoint.dX <= dMaxX &&
        oPoint.dY >= dMinY && oPoint.dY <= dMaxY;
}

bool Envelope::intersects(const Envelope& oOther) const {
    return !(oOther.dMaxX < dMinX || oOther.dMinX > dMaxX ||
        oOther.dMaxY < dMinY || oOther.dMinY > dMaxY);
}

double Envelope::getWidth() const {
    return dMaxX - dMinX;
}

double Envelope::getHeight() const {
    return dMaxY - dMinY;
}

Point Envelope::getCenter() const {
    return Point((dMinX + dMaxX) / 2.0, (dMinY + dMaxY) / 2.0);
}

/* ========== Geometry 实现 ========== */

Geometry::Geometry() : eType(GeometryType::POINT), oPoint() {}

Geometry::Geometry(const Point& oPointVal) : eType(GeometryType::POINT), oPoint(oPointVal) {}

Geometry::Geometry(const LineString& oLineStringVal) : eType(GeometryType::LINE_STRING), oLineString(oLineStringVal) {}

Geometry::Geometry(const Polygon& oPolygonVal) : eType(GeometryType::POLYGON), oPolygon(oPolygonVal) {}

Geometry::Geometry(const Envelope& oEnvelopeVal) : eType(GeometryType::ENVELOPE), oEnvelope(oEnvelopeVal) {}

Geometry::Geometry(const Geometry& oOther) : eType(oOther.eType) {
    switch (eType) {
    case GeometryType::POINT:
        new (&oPoint) Point(oOther.oPoint);
        break;
    case GeometryType::LINE_STRING:
        new (&oLineString) LineString(oOther.oLineString);
        break;
    case GeometryType::POLYGON:
        new (&oPolygon) Polygon(oOther.oPolygon);
        break;
    case GeometryType::ENVELOPE:
        new (&oEnvelope) Envelope(oOther.oEnvelope);
        break;
    }
}

Geometry& Geometry::operator=(const Geometry& oOther) {
    if (this != &oOther) {
        this->~Geometry();
        eType = oOther.eType;
        switch (eType) {
        case GeometryType::POINT:
            new (&oPoint) Point(oOther.oPoint);
            break;
        case GeometryType::LINE_STRING:
            new (&oLineString) LineString(oOther.oLineString);
            break;
        case GeometryType::POLYGON:
            new (&oPolygon) Polygon(oOther.oPolygon);
            break;
        case GeometryType::ENVELOPE:
            new (&oEnvelope) Envelope(oOther.oEnvelope);
            break;
        }
    }
    return *this;
}

Geometry::~Geometry() {
    switch (eType) {
    case GeometryType::POINT:
        oPoint.~Point();
        break;
    case GeometryType::LINE_STRING:
        oLineString.~LineString();
        break;
    case GeometryType::POLYGON:
        oPolygon.~Polygon();
        break;
    case GeometryType::ENVELOPE:
        oEnvelope.~Envelope();
        break;
    }
}

Envelope Geometry::getEnvelope() const {
    switch (eType) {
    case GeometryType::POINT:
        return Envelope(oPoint.dX, oPoint.dY, oPoint.dX, oPoint.dY);
    case GeometryType::LINE_STRING:
        return oLineString.getEnvelope();
    case GeometryType::POLYGON:
        return oPolygon.getEnvelope();
    case GeometryType::ENVELOPE:
        return oEnvelope;
    }
    return Envelope();
}

/* 【统一接口】WKT转换 - 供队友数据库模块使用 */
std::string Geometry::toWKT() const {
    std::ostringstream oStream;
    switch (eType) {
    case GeometryType::POINT:
        oStream << "POINT(" << oPoint.dX << " " << oPoint.dY << ")";
        break;
    case GeometryType::LINE_STRING: {
        oStream << "LINESTRING(";
        int nCount = oLineString.getPointCount();
        for (int nIdx = 0; nIdx < nCount; ++nIdx) {
            if (nIdx > 0) oStream << ",";
            oStream << oLineString.vPoints[nIdx].dX << " "
                << oLineString.vPoints[nIdx].dY;
        }
        oStream << ")";
        break;
    }
    case GeometryType::POLYGON: {
        oStream << "POLYGON(";
        int nRingCount = static_cast<int>(oPolygon.vRings.size());
        for (int nRingIdx = 0; nRingIdx < nRingCount; ++nRingIdx) {
            if (nRingIdx > 0) oStream << ",";
            oStream << "(";
            const auto& vPts = oPolygon.vRings[nRingIdx].vPoints;
            for (size_t nIdx = 0; nIdx < vPts.size(); ++nIdx) {
                if (nIdx > 0) oStream << ",";
                oStream << vPts[nIdx].dX << " " << vPts[nIdx].dY;
            }
            oStream << ")";
        }
        oStream << ")";
        break;
    }
    default:
        oStream << "EMPTY";
        break;
    }
    return oStream.str();
}

/* 【统一接口】从WKT解析 - 供队友数据库模块使用 */
Geometry Geometry::fromWKT(const std::string& sWKT) {
    /* 简化实现：仅支持POINT */
    if (sWKT.find("POINT") != std::string::npos) {
        double dX = 0.0, dY = 0.0;
        std::sscanf(sWKT.c_str(), "POINT(%lf %lf)", &dX, &dY);
        return Geometry(Point(dX, dY));
    }
    return Geometry(Point(0.0, 0.0));
}
