#include "SpatialQuery.h"
#include <QStringList>

// ============================================================
// 空间函数封装
// ============================================================

QString SpatialQuery::stBuffer(const QString& geomColumn, double distance)
{
    return QString("ST_Buffer(%1, %2)").arg(geomColumn).arg(distance);
}

QString SpatialQuery::stIntersects(const QString& geomA, const QString& geomB)
{
    return QString("ST_Intersects(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stContains(const QString& geomA, const QString& geomB)
{
    return QString("ST_Contains(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stWithin(const QString& geomA, const QString& geomB)
{
    return QString("ST_Within(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stIntersection(const QString& geomA, const QString& geomB)
{
    return QString("ST_Intersection(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stUnion(const QString& geomA, const QString& geomB)
{
    return QString("ST_Union(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stDifference(const QString& geomA, const QString& geomB)
{
    return QString("ST_Difference(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stDistance(const QString& geomA, const QString& geomB)
{
    return QString("ST_Distance(%1, %2)").arg(geomA, geomB);
}

QString SpatialQuery::stArea(const QString& geomColumn)
{
    return QString("ST_Area(%1)").arg(geomColumn);
}

QString SpatialQuery::stLength(const QString& geomColumn)
{
    return QString("ST_Length(%1)").arg(geomColumn);
}

QString SpatialQuery::stEnvelope(const QString& geomColumn)
{
    return QString("ST_Envelope(%1)").arg(geomColumn);
}

QString SpatialQuery::stTransform(const QString& geomColumn, int sourceSrid, int targetSrid)
{
    Q_UNUSED(sourceSrid);
    return QString("ST_Transform(%1, %2)").arg(geomColumn).arg(targetSrid);
}

QString SpatialQuery::stCentroid(const QString& geomColumn)
{
    return QString("ST_Centroid(%1)").arg(geomColumn);
}


// ============================================================
// 完整查询构造
// ============================================================

QString SpatialQuery::selectByExtent(const QString& tableName,
                                      const QString& geomColumn,
                                      const QgsRectangle& extent)
{
    QString bbox = QString("BuildMbr(%1, %2, %3, %4)")
        .arg(extent.xMinimum(), 0, 'f', 10)
        .arg(extent.yMinimum(), 0, 'f', 10)
        .arg(extent.xMaximum(), 0, 'f', 10)
        .arg(extent.yMaximum(), 0, 'f', 10);

    return QString("SELECT * FROM \"%1\" WHERE %2 = 1")
        .arg(tableName, stIntersects(geomColumn, bbox));
}

QString SpatialQuery::selectByAttribute(const QString& tableName, const QString& filter)
{
    return QString("SELECT * FROM \"%1\" WHERE %2").arg(tableName, filter);
}

QString SpatialQuery::selectBuffered(const QString& tableName,
                                      const QString& geomColumn, double distance)
{
    return QString("SELECT *, %1 AS buffered_geom FROM \"%2\"")
        .arg(stBuffer(geomColumn, distance), tableName);
}

QString SpatialQuery::selectIntersection(const QString& tableA, const QString& tableB,
                                          const QString& geomA, const QString& geomB,
                                          const QString& resultColumns)
{
    return QString(
        "SELECT %1, %2 AS intersection_geom "
        "FROM \"%3\" AS a, \"%4\" AS b "
        "WHERE %5"
    ).arg(resultColumns,
          stIntersection("a." + geomA, "b." + geomB),
          tableA, tableB,
          stIntersects("a." + geomA, "b." + geomB));
}

QString SpatialQuery::selectNearby(const QString& targetTable, const QString& refTable,
                                     const QString& targetGeom, const QString& refGeom,
                                     double distance)
{
    return QString(
        "SELECT t.* FROM \"%1\" AS t, \"%2\" AS r "
        "WHERE %3 < %4"
    ).arg(targetTable, refTable,
          stDistance("t." + targetGeom, "r." + refGeom),
          QString::number(distance));
}

QString SpatialQuery::bufferAndIntersect(const QString& targetTable, const QString& refTable,
                                           const QString& targetGeom, const QString& refGeom,
                                           double distance)
{
    return QString(
        "SELECT t.* FROM \"%1\" AS t, \"%2\" AS r "
        "WHERE %3 AND %4"
    ).arg(targetTable, refTable,
          stIntersects("t." + targetGeom, stBuffer("r." + refGeom, distance)),
          QString("r.ROWID IS NOT NULL"));
}

QString SpatialQuery::createTableAs(const QString& sql, const QString& newTableName)
{
    return QString("CREATE TABLE \"%1\" AS %2").arg(newTableName, sql);
}

QString SpatialQuery::insertFromQuery(const QString& targetTable, const QString& selectSql)
{
    return QString("INSERT INTO \"%1\" %2").arg(targetTable, selectSql);
}

QString SpatialQuery::initSpatialMetadata()
{
    return "SELECT InitSpatialMetaData(1);";
}
