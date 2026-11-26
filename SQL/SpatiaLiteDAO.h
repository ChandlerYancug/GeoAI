#pragma once

#include "DataAccessObject.h"
#include <QSqlDatabase>
#include <QSqlQuery>

// ============================================================
// SpatiaLiteDAO — SpatiaLite 数据访问实现
//   OOP: DataAccessObject 派生类，封装 Qt QSqlDatabase
//   使用SQLite驱动连接SpatiaLite数据库
// ============================================================
class SpatiaLiteDAO : public DataAccessObject
{
public:
    SpatiaLiteDAO();
    ~SpatiaLiteDAO() override;

    // —— 连接管理 ——
    bool connect(const QString& dbPath) override;
    void disconnect() override;
    bool isConnected() const override;
    QString connectionString() const override;
    QString lastError() const { return mLastError; }

    // —— 要素读取 ——
    QList<QgsFeature> readFeatures(const QString& layerName,
                                    const QString& filter = QString()) override;
    QgsFeature readFeature(const QString& layerName, qint64 featureId) override;

    // —— 要素写入 ——
    bool writeFeatures(const QString& layerName,
                        const QList<QgsFeature>& features) override;
    bool createLayer(const QString& layerName,
                      const QString& geometryType,
                      const QgsFields& fields,
                      int srid = 4326) override;

    // —— 空间SQL ——
    QList<QgsFeature> executeSpatialSQL(const QString& sql) override;

    // —— 空间操作封装 ——
    QList<QgsFeature> buffer(const QString& layerName, double distance) override;
    QList<QgsFeature> intersect(const QString& layerA, const QString& layerB) override;
    QList<QgsFeature> filterByExtent(const QString& layerName,
                                      const QgsRectangle& extent) override;
    QList<QgsFeature> filterByAttribute(const QString& layerName,
                                         const QString& expression) override;

    // —— 元数据 ——
    QStringList tableNames() const override;
    QgsFields fields(const QString& layerName) const override;

private:
    QSqlDatabase mDb;
    QString mDbPath;
    bool mConnected = false;
    mutable QString mLastError;

    // 内部辅助
    QgsFeature featureFromQuery(const QSqlQuery& query) const;
    QString geometryColumn(const QString& tableName) const;
    QString geometryTypeFromName(const QString& geomType) const;
    QString wkbTypeToGeometryTypeName(int wkbType) const;
};
