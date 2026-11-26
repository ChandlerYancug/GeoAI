/* FileName: VectorLayer.h
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 矢量图层接口 - 供模块二/三使用
*/

#ifndef VECTORLAYER_H
#define VECTORLAYER_H

#include "MapLayer.h"
#include "Geometry.h"
#include <vector>

class VectorLayer : public MapLayer {
public:
    virtual ~VectorLayer() = default;

    /* 获取要素数量 */
    virtual int getFeatureCount() const = 0;

    /* 获取所有几何体（模块三核心调用） */
    virtual std::vector<Geometry> getFeatures() const = 0;

    /* 获取指定索引的几何体 */
    virtual Geometry getFeatureAt(int nIndex) const = 0;

    /* 数据库对接（模块二调用） */
    virtual bool importFromDatabase(const QString& sTableName) = 0;
    virtual bool exportToDatabase(const QString& sTableName) = 0;
    static VectorLayer* createFromFile(const QString& sPath, const QString& sName);
};

#endif