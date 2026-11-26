#pragma once

#include <QString>
#include <QObject>
#include <memory>

// ============================================================
// DataSource — 数据源抽象基类
//   OOP: 适配器模式 — 将不同数据源API适配为统一接口
//   OOP: 工厂模式 — 通过静态工厂方法创建具体实例
// ============================================================
class DataSource : public QObject
{
    Q_OBJECT

public:
    enum SourceType { File, Database, WebService };

    explicit DataSource(QObject* parent = nullptr);
    virtual ~DataSource();

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual SourceType sourceType() const = 0;

    virtual QString uri() const = 0;
    virtual QString providerName() const = 0;

    // 工厂方法 — 从URI自动判断类型并创建
    static std::unique_ptr<DataSource> createFromUri(const QString& uri);

signals:
    void connectionStatusChanged(bool connected);

protected:
    bool mConnected = false;
};


// ============================================================
// FileDataSource — 文件数据源
// ============================================================
class FileDataSource : public DataSource
{
    Q_OBJECT

public:
    explicit FileDataSource(const QString& filePath, QObject* parent = nullptr);
    ~FileDataSource() override;

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    SourceType sourceType() const override { return File; }

    QString uri() const override;
    QString filePath() const;
    QString providerName() const;  // "ogr", "gdal", etc.

private:
    QString mFilePath;
    QString mProviderName;

    // 根据文件扩展名判断Provider
    void detectProvider();
};


// ============================================================
// DatabaseDataSource — 数据库数据源
// ============================================================
class DatabaseDataSource : public DataSource
{
    Q_OBJECT

public:
    enum DbType { SpatiaLite, PostGIS, GeoPackage };

    DatabaseDataSource(DbType type, const QString& dbPath,
                       const QString& tableName, QObject* parent = nullptr);
    ~DatabaseDataSource() override;

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    SourceType sourceType() const override { return Database; }

    QString uri() const override;
    QString providerName() const override;
    DbType dbType() const;
    QString tableName() const;

    // 构建QGIS可识别的数据库URI
    static QString buildUri(DbType type, const QString& dbPath, const QString& tableName);

private:
    DbType mDbType;
    QString mDbPath;
    QString mTableName;
};
