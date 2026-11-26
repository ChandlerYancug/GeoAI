#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>

// ============================================================
// DataAccessObject — 数据访问对象抽象基类
//   OOP: 工厂模式 — 通过静态工厂方法创建具体DAO实例
//   OOP: 依赖倒置 — 上层分析引擎只依赖此抽象接口
// ============================================================
class DataAccessObject
{
public:
    virtual ~DataAccessObject() = default;

    // —— 连接管理 ——
    virtual bool connect(const QString& connectionString) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual QString connectionString() const = 0;

    // —— 要素读取 ——
    virtual QList<QgsFeature> readFeatures(const QString& layerName,
                                            const QString& filter = QString()) = 0;
    virtual QgsFeature readFeature(const QString& layerName, qint64 featureId) = 0;

    // —— 要素写入 ——
    virtual bool writeFeatures(const QString& layerName,
                                const QList<QgsFeature>& features) = 0;
    virtual bool createLayer(const QString& layerName,
                              const QString& geometryType,
                              const QgsFields& fields,
                              int srid = 4326) = 0;

    // —— 空间SQL ——
    virtual QList<QgsFeature> executeSpatialSQL(const QString& sql) = 0;

    // —— 空间操作封装 ——
    virtual QList<QgsFeature> buffer(const QString& layerName, double distance) = 0;
    virtual QList<QgsFeature> intersect(const QString& layerA, const QString& layerB) = 0;
    virtual QList<QgsFeature> filterByExtent(const QString& layerName,
                                              const QgsRectangle& extent) = 0;
    virtual QList<QgsFeature> filterByAttribute(const QString& layerName,
                                                 const QString& expression) = 0;

    // —— 元数据 ——
    virtual QStringList tableNames() const = 0;
    virtual QgsFields fields(const QString& layerName) const = 0;

    // 工厂方法
    enum DatabaseType { SpatiaLite, PostGIS, GeoPackage };
    static DataAccessObject* create(DatabaseType type);
};
