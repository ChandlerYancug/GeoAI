#pragma once

#include <QObject>
#include <vector>
#include <memory>
#include "MapLayer.h"

// ============================================================
// LayerManager — 图层管理器
//   OOP: 单一职责 — 只负责图层的增删查改和顺序管理
//   不涉及渲染、分析等职责
// ============================================================
class LayerManager : public QObject
{
    Q_OBJECT

public:
    explicit LayerManager(QObject* parent = nullptr);
    ~LayerManager();

    // 图层增删
    void addLayer(std::unique_ptr<MapLayer> layer);
    void removeLayer(const QString& id);
    void removeLayer(int index);
    void clear();

    // 图层查询
    MapLayer* layerById(const QString& id) const;
    MapLayer* layerByName(const QString& name) const;
    MapLayer* layerAt(int index) const;
    int layerCount() const;
    QList<MapLayer*> allLayers() const;
    QList<VectorLayer*> vectorLayers() const;
    QList<RasterLayer*> rasterLayers() const;

    // 图层顺序
    void moveLayerUp(int index);
    void moveLayerDown(int index);
    void moveLayer(int fromIndex, int toIndex);

    // 图层可见性
    void setLayerVisible(const QString& id, bool visible);

    // 查找匹配名称的图层（支持模糊匹配）
    MapLayer* findLayerByKeyword(const QString& keyword) const;

signals:
    void layerAdded(MapLayer* layer);
    void layerRemoved(const QString& id);
    void layerOrderChanged();
    void layersCleared();

private:
    std::vector<std::unique_ptr<MapLayer>> mLayers;
};
