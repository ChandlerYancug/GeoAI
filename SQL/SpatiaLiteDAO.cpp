#include "SpatiaLiteDAO.h"
#include "SpatialQuery.h"
#include "SqlHelper.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>
#include <QVariant>
#include <QFileInfo>
#include <QDebug>
#include <qgsgeometry.h>
#include <qgswkbtypes.h>

// ============================================================
// SpatiaLiteDAO 实现
// ============================================================

SpatiaLiteDAO::SpatiaLiteDAO()
{
}

SpatiaLiteDAO::~SpatiaLiteDAO()
{
    disconnect();
}

bool SpatiaLiteDAO::connect(const QString& dbPath)
{
    if (mConnected && mDb.isOpen())
        return true;

    QFileInfo fi(dbPath);
    if (!fi.exists())
    {
        mLastError = QString("文件不存在: %1").arg(dbPath);
        return false;
    }

    mDbPath = dbPath;

    // Use unique connection name
    QString connName = QString("spatialite_%1").arg(
        reinterpret_cast<quintptr>(this), 0, 16);

    // Remove any stale connection with this name
    if (QSqlDatabase::contains(connName))
        QSqlDatabase::removeDatabase(connName);

    mDb = QSqlDatabase::addDatabase("QSQLITE", connName);
    if (!mDb.isValid())
    {
        mLastError = "QSQLITE driver not available";
        return false;
    }

    mDb.setDatabaseName(dbPath);

    if (!mDb.open())
    {
        mLastError = mDb.lastError().text();
        return false;
    }

    // Try loading SpatiaLite extension (non-fatal if it fails)
    QSqlQuery query(mDb);
    bool hasSpatialite = query.exec("SELECT load_extension('mod_spatialite')");
    if (!hasSpatialite)
        hasSpatialite = query.exec("SELECT load_extension('libspatialite')");
    if (!hasSpatialite)
        qDebug() << "SpatiaLite extension not loaded - spatial functions unavailable";
    
    // Initialize spatial metadata if available
    query.exec("SELECT InitSpatialMetaData(1)");

    mConnected = true;
    return true;
}

void SpatiaLiteDAO::disconnect()
{
    if (mConnected)
    {
        QString connName = mDb.connectionName();
        mDb.close();
        mDb = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName);
        mConnected = false;
    }
}

bool SpatiaLiteDAO::isConnected() const
{
    return mConnected && mDb.isOpen();
}

QString SpatiaLiteDAO::connectionString() const
{
    return mDbPath;
}


// ============================================================
// 要素读取
// ============================================================

QList<QgsFeature> SpatiaLiteDAO::readFeatures(const QString& layerName,
                                                const QString& filter)
{
    QList<QgsFeature> features;
    if (!isConnected()) return features;

    QString sql = QString("SELECT * FROM %1").arg(SqlHelper::quoteIdentifier(layerName));
    if (!filter.isEmpty() && SqlHelper::isSafeWhereClause(filter))
        sql += " WHERE " + filter;

    QSqlQuery query(mDb);
    if (query.exec(sql))
    {
        while (query.next())
        {
            QgsFeature feat = featureFromQuery(query);
            if (feat.isValid())
                features.append(feat);
        }
    }

    return features;
}

QgsFeature SpatiaLiteDAO::readFeature(const QString& layerName, qint64 featureId)
{
    QgsFeature feat;
    if (!isConnected()) return feat;

    QSqlQuery query(mDb);
    query.prepare(QString("SELECT * FROM \"%1\" WHERE ROWID = ?").arg(layerName));
    query.addBindValue(featureId);

    if (query.exec() && query.next())
    {
        feat = featureFromQuery(query);
    }

    return feat;
}


// ============================================================
// 要素写入
// ============================================================

bool SpatiaLiteDAO::writeFeatures(const QString& layerName,
                                   const QList<QgsFeature>& features)
{
    if (!isConnected()) return false;

    QString geomCol = geometryColumn(layerName);
    if (geomCol.isEmpty()) return false;

    QSqlQuery query(mDb);

    for (const QgsFeature& feat : features)
    {
        if (!feat.hasGeometry()) continue;

        QString wkt = feat.geometry().asWkt();
        QString sql = QString(
            "INSERT INTO \"%1\" (\"%2\") VALUES (GeomFromText('%3', 4326))"
        ).arg(layerName, geomCol, wkt);

        if (!query.exec(sql))
        {
            qWarning("SpatiaLiteDAO::writeFeatures: %s",
                     qPrintable(query.lastError().text()));
            return false;
        }
    }

    return true;
}

bool SpatiaLiteDAO::createLayer(const QString& layerName,
                                 const QString& geometryType,
                                 const QgsFields& fields,
                                 int srid)
{
    if (!isConnected()) return false;

    QString sql = QString(
        "CREATE TABLE \"%1\" ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT"
    ).arg(layerName);

    // 添加属性字段
    for (int i = 0; i < fields.size(); ++i)
    {
        const QgsField& f = fields.at(i);
        QString colName = f.name();
        QString colType = "TEXT";  // 简化处理
        if (f.type() == QVariant::Int || f.type() == QVariant::LongLong)
            colType = "INTEGER";
        else if (f.type() == QVariant::Double)
            colType = "REAL";

        sql += QString(", \"%1\" %2").arg(colName, colType);
    }

    sql += ")";

    QSqlQuery query(mDb);
    if (!query.exec(sql))
        return false;

    // 添加几何字段
    QString geomSql = QString(
        "SELECT AddGeometryColumn('%1', 'geom', %2, '%3', 2)"
    ).arg(layerName).arg(srid).arg(geometryType);

    if (!query.exec(geomSql))
        return false;

    return true;
}


// ============================================================
// 空间SQL
// ============================================================

QList<QgsFeature> SpatiaLiteDAO::executeSpatialSQL(const QString& sql)
{
    QList<QgsFeature> features;
    if (!isConnected()) return features;

    QSqlQuery query(mDb);
    if (query.exec(sql))
    {
        while (query.next())
        {
            QgsFeature feat = featureFromQuery(query);
            if (feat.isValid())
                features.append(feat);
        }
    }
    else
    {
        qWarning("SpatiaLiteDAO::executeSpatialSQL error: %s",
                 qPrintable(query.lastError().text()));
    }

    return features;
}


// ============================================================
// 空间操作封装
// ============================================================

QList<QgsFeature> SpatiaLiteDAO::buffer(const QString& layerName, double distance)
{
    QString geomCol = geometryColumn(layerName);
    QString sql = SpatialQuery::selectBuffered(layerName, geomCol, distance);
    return executeSpatialSQL(sql);
}

QList<QgsFeature> SpatiaLiteDAO::intersect(const QString& layerA, const QString& layerB)
{
    QString geomA = geometryColumn(layerA);
    QString geomB = geometryColumn(layerB);
    QString sql = SpatialQuery::selectIntersection(layerA, layerB, geomA, geomB);
    return executeSpatialSQL(sql);
}

QList<QgsFeature> SpatiaLiteDAO::filterByExtent(const QString& layerName,
                                                  const QgsRectangle& extent)
{
    QString geomCol = geometryColumn(layerName);
    QString sql = SpatialQuery::selectByExtent(layerName, geomCol, extent);
    return executeSpatialSQL(sql);
}

QList<QgsFeature> SpatiaLiteDAO::filterByAttribute(const QString& layerName,
                                                     const QString& expression)
{
    QString sql = SpatialQuery::selectByAttribute(layerName, expression);
    return executeSpatialSQL(sql);
}


// ============================================================
// 元数据
// ============================================================

QStringList SpatiaLiteDAO::tableNames() const
{
    QStringList tables;
    if (!isConnected()) return tables;

    QSqlQuery query(mDb);
    query.exec("SELECT f_table_name FROM geometry_columns");

    while (query.next())
        tables.append(query.value(0).toString());

    return tables;
}

QgsFields SpatiaLiteDAO::fields(const QString& layerName) const
{
    QgsFields fields;
    if (!isConnected()) return fields;

    QSqlQuery query(mDb);
    query.exec(QString("PRAGMA table_info(\"%1\")").arg(layerName));

    while (query.next())
    {
        QString name = query.value("name").toString();
        QString type = query.value("type").toString().toUpper();

        QVariant::Type qType = QVariant::String;
        if (type == "INTEGER" || type == "INT")
            qType = QVariant::Int;
        else if (type == "REAL" || type == "DOUBLE" || type == "FLOAT")
            qType = QVariant::Double;

        if (name.toLower() != "geom" && name.toLower() != "geometry")
            fields.append(QgsField(name, qType));
    }

    return fields;
}


// ============================================================
// 内部辅助方法
// ============================================================

QgsFeature SpatiaLiteDAO::featureFromQuery(const QSqlQuery& query) const
{
    QgsFeature feat;
    QgsFields fields;

    QSqlRecord record = query.record();

    // 初始化字段
    for (int i = 0; i < record.count(); ++i)
    {
        QString fieldName = record.fieldName(i);
        if (fieldName.toLower() != "geom" && fieldName.toLower() != "geometry")
        {
            QVariant::Type qType = record.field(i).type();
            if (qType == QVariant::Int || qType == QVariant::LongLong)
                fields.append(QgsField(fieldName, QVariant::Int));
            else if (qType == QVariant::Double)
                fields.append(QgsField(fieldName, QVariant::Double));
            else
                fields.append(QgsField(fieldName, QVariant::String));
        }
    }

    feat.setFields(fields);

    // 设置属性值
    for (int i = 0; i < record.count(); ++i)
    {
        QString fieldName = record.fieldName(i);
        if (fieldName.toLower() != "geom" && fieldName.toLower() != "geometry")
        {
            int fieldIdx = fields.indexOf(fieldName);
            if (fieldIdx >= 0)
                feat.setAttribute(fieldIdx, query.value(i));
        }
    }

    // 设置几何（尝试多种常见列名）
    QStringList geomCandidates = {"geom", "geometry", "GEOMETRY", "Geom"};
    for (const QString& col : geomCandidates)
    {
        int idx = record.indexOf(col);
        if (idx >= 0)
        {
            QByteArray wkb = query.value(idx).toByteArray();
            if (!wkb.isEmpty())
            {
                QgsGeometry geom;
                geom.fromWkb(wkb);
                if (!geom.isNull())
                {
                    feat.setGeometry(geom);
                    feat.setValid(true);
                    break;
                }
            }
        }
    }

    // 如果没找到几何列，尝试从第一个BLOB类型列读取
    if (!feat.hasGeometry())
    {
        for (int i = 0; i < record.count(); ++i)
        {
            if (record.field(i).type() == QVariant::ByteArray)
            {
                QByteArray wkb = query.value(i).toByteArray();
                if (wkb.size() > 10)  // 合理的WKB最小大小
                {
                    QgsGeometry geom;
                    geom.fromWkb(wkb);
                    if (!geom.isNull())
                    {
                        feat.setGeometry(geom);
                        feat.setValid(true);
                        break;
                    }
                }
            }
        }
    }

    return feat;
}

QString SpatiaLiteDAO::geometryColumn(const QString& tableName) const
{
    if (!isConnected()) return "geom";

    QSqlQuery query(mDb);
    query.prepare("SELECT f_geometry_column FROM geometry_columns WHERE f_table_name = ?");
    query.addBindValue(tableName);

    if (query.exec() && query.next())
        return query.value(0).toString();

    return "geom";  // 默认值
}

QString SpatiaLiteDAO::geometryTypeFromName(const QString& geomType) const
{
    QString t = geomType.toUpper();
    if (t == "POINT" || t == "MULTIPOINT")           return "POINT";
    if (t == "LINESTRING" || t == "MULTILINESTRING")  return "LINESTRING";
    if (t == "POLYGON" || t == "MULTIPOLYGON")        return "POLYGON";
    return "GEOMETRY";
}

QString SpatiaLiteDAO::wkbTypeToGeometryTypeName(int wkbType) const
{
    switch (QgsWkbTypes::flatType(static_cast<QgsWkbTypes::Type>(wkbType)))
    {
    case QgsWkbTypes::Point:           return "POINT";
    case QgsWkbTypes::LineString:      return "LINESTRING";
    case QgsWkbTypes::Polygon:         return "POLYGON";
    case QgsWkbTypes::MultiPoint:      return "MULTIPOINT";
    case QgsWkbTypes::MultiLineString: return "MULTILINESTRING";
    case QgsWkbTypes::MultiPolygon:    return "MULTIPOLYGON";
    default:                           return "GEOMETRY";
    }
}
