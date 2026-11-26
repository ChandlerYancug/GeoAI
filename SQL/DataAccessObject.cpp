#include "DataAccessObject.h"
#include "SpatiaLiteDAO.h"

DataAccessObject* DataAccessObject::create(DatabaseType type)
{
    switch (type)
    {
    case SpatiaLite:
        return new SpatiaLiteDAO();
    case PostGIS:
        // 暂未实现PostGIS DAO，返回SpatiaLite作为fallback
        return new SpatiaLiteDAO();
    case GeoPackage:
        return new SpatiaLiteDAO();  // GeoPackage也用SQLite驱动
    }
    return new SpatiaLiteDAO();
}
