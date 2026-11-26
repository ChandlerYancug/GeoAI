/* FileName: LayerManager.h
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 图层管理器单例 - 供模块三/四/五使用
*/

#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include "MapLayer.h"
#include <vector>
#include <QString>

class LayerManager {
public:
    /* 获取单例实例 */
    static LayerManager& getInstance();

    /* 禁止拷贝 */
    LayerManager(const LayerManager&) = delete;
    LayerManager& operator=(const LayerManager&) = delete;

    /* 添加/移除图层 */
    void addLayer(MapLayer* pLayer);
    void removeLayer(const QString& sLayerName);

    /* 按名称查找图层（模块四用） */
    MapLayer* getLayerByName(const QString& sName);

    /* 获取所有图层 */
    std::vector<MapLayer*> getAllLayers();

    /* 刷新地图显示（模块三用） */
    void refreshMap();

private:
    LayerManager() = default;
    ~LayerManager() = default;

    std::vector<MapLayer*> m_vLayers;
};

#endif