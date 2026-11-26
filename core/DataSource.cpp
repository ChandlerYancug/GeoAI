#include "DataSource.h"
#include <QFileInfo>
#include <QUrl>

// ============================================================
// DataSource 实现
// ============================================================

DataSource::DataSource(QObject* parent) : QObject(parent) {}
DataSource::~DataSource() = default;

std::unique_ptr<DataSource> DataSource::createFromUri(const QString& uri)
{
    QFileInfo fi(uri);

    // 文件数据源
    if (fi.exists() && fi.isFile())
    {
        return std::make_unique<FileDataSource>(uri);
    }

    // 数据库连接字符串（简化判断）
    if (uri.startsWith("db:") || uri.startsWith("spatialite:") ||
        uri.startsWith("postgis:") || uri.startsWith("geopackage:"))
    {
        // 解析数据库类型和参数
        // 格式: "db:spatialite:/path/to/db|tablename"
        QStringList parts = uri.mid(3).split('|');
        if (parts.size() >= 2)
        {
            DatabaseDataSource::DbType dbType;
            QString typeStr = parts[0];
            if (typeStr == "spatialite")       dbType = DatabaseDataSource::SpatiaLite;
            else if (typeStr == "postgis")     dbType = DatabaseDataSource::PostGIS;
            else if (typeStr == "geopackage")  dbType = DatabaseDataSource::GeoPackage;
            else dbType = DatabaseDataSource::SpatiaLite;

            return std::make_unique<DatabaseDataSource>(dbType, parts[0], parts[1]);
        }
    }

    // 默认作为文件处理
    return std::make_unique<FileDataSource>(uri);
}


// ============================================================
// FileDataSource 实现
// ============================================================

FileDataSource::FileDataSource(const QString& filePath, QObject* parent)
    : DataSource(parent)
    , mFilePath(filePath)
{
    detectProvider();
}

FileDataSource::~FileDataSource()
{
    disconnect();
}

void FileDataSource::detectProvider()
{
    QString suffix = QFileInfo(mFilePath).suffix().toLower();

    // OGR支持的矢量格式
    if (suffix == "shp" || suffix == "geojson" || suffix == "json" ||
        suffix == "gpkg" || suffix == "kml" || suffix == "gml" ||
        suffix == "tab" || suffix == "mif")
    {
        mProviderName = "ogr";
    }
    // GDAL支持的栅格格式
    else if (suffix == "tif" || suffix == "tiff" || suffix == "img" ||
             suffix == "dem" || suffix == "asc" || suffix == "bil")
    {
        mProviderName = "gdal";
    }
    else
    {
        // 默认尝试OGR
        mProviderName = "ogr";
    }
}

bool FileDataSource::connect()
{
    if (mConnected) return true;
    QFileInfo fi(mFilePath);
    if (fi.exists())
    {
        mConnected = true;
        emit connectionStatusChanged(true);
        return true;
    }
    return false;
}

void FileDataSource::disconnect()
{
    mConnected = false;
    emit connectionStatusChanged(false);
}

bool FileDataSource::isConnected() const { return mConnected; }
QString FileDataSource::uri() const { return mFilePath; }
QString FileDataSource::filePath() const { return mFilePath; }
QString FileDataSource::providerName() const { return mProviderName; }


// ============================================================
// DatabaseDataSource 实现
// ============================================================

DatabaseDataSource::DatabaseDataSource(DbType type, const QString& dbPath,
                                       const QString& tableName, QObject* parent)
    : DataSource(parent)
    , mDbType(type)
    , mDbPath(dbPath)
    , mTableName(tableName)
{
}

DatabaseDataSource::~DatabaseDataSource()
{
    disconnect();
}

bool DatabaseDataSource::connect()
{
    if (mConnected) return true;
    QFileInfo fi(mDbPath);
    if (fi.exists() || mDbType == PostGIS)
    {
        mConnected = true;
        emit connectionStatusChanged(true);
        return true;
    }
    return false;
}

void DatabaseDataSource::disconnect()
{
    mConnected = false;
    emit connectionStatusChanged(false);
}

bool DatabaseDataSource::isConnected() const { return mConnected; }

QString DatabaseDataSource::uri() const
{
    return buildUri(mDbType, mDbPath, mTableName);
}

DatabaseDataSource::DbType DatabaseDataSource::dbType() const { return mDbType; }
QString DatabaseDataSource::tableName() const { return mTableName; }

QString DatabaseDataSource::providerName() const
{
    switch (mDbType)
    {
    case SpatiaLite:
    case GeoPackage:
        return "ogr";       // QGIS 3.x uses OGR/GDAL driver for SpatiaLite & GeoPackage
    case PostGIS:
        return "postgres";
    }
    return "ogr";
}

QString DatabaseDataSource::buildUri(DbType type, const QString& dbPath, const QString& tableName)
{
    // Normalize path separators to forward slashes (required by GDAL/OGR on all platforms)
    QString path = QString(dbPath).replace('\\', '/');

    switch (type)
    {
    case SpatiaLite:
    case GeoPackage:
        // OGR SQLite datasource format: path|layername=table_name
        return QString("%1|layername=%2").arg(path, tableName);
    case PostGIS:
        return path;  // dbPath should be a full PG connection string
    }
    return path;
}
