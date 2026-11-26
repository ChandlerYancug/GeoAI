#pragma once

#include <QString>
#include <qgsrectangle.h>

// ============================================================
// SpatialQuery — 空间SQL构造器
//   封装常见空间SQL模板，生成符合SpatiaLite语法的SQL
//   OOP: 工具类，单一职责——只负责SQL字符串构造
// ============================================================
class SpatialQuery
{
public:
    // 空间参考函数
    static QString stBuffer(const QString& geomColumn, double distance);
    static QString stIntersects(const QString& geomA, const QString& geomB);
    static QString stContains(const QString& geomA, const QString& geomB);
    static QString stWithin(const QString& geomA, const QString& geomB);
    static QString stIntersection(const QString& geomA, const QString& geomB);
    static QString stUnion(const QString& geomA, const QString& geomB);
    static QString stDifference(const QString& geomA, const QString& geomB);
    static QString stDistance(const QString& geomA, const QString& geomB);
    static QString stArea(const QString& geomColumn);
    static QString stLength(const QString& geomColumn);
    static QString stEnvelope(const QString& geomColumn);
    static QString stTransform(const QString& geomColumn, int sourceSrid, int targetSrid);
    static QString stCentroid(const QString& geomColumn);

    // 构造完整查询
    static QString selectByExtent(const QString& tableName,
                                   const QString& geomColumn,
                                   const QgsRectangle& extent);
    static QString selectByAttribute(const QString& tableName, const QString& filter);
    static QString selectBuffered(const QString& tableName,
                                   const QString& geomColumn,
                                   double distance);
    static QString selectIntersection(const QString& tableA, const QString& tableB,
                                       const QString& geomA, const QString& geomB,
                                       const QString& resultColumns = "*");
    static QString selectNearby(const QString& targetTable, const QString& refTable,
                                  const QString& targetGeom, const QString& refGeom,
                                  double distance);
    static QString bufferAndIntersect(const QString& targetTable, const QString& refTable,
                                        const QString& targetGeom, const QString& refGeom,
                                        double distance);
    static QString createTableAs(const QString& sql, const QString& newTableName);
    static QString insertFromQuery(const QString& targetTable, const QString& selectSql);

    // 初始化SpatiaLite扩展
    static QString initSpatialMetadata();
};
