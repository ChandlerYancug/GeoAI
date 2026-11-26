#include "LayerManager.h"
#include <algorithm>

LayerManager::LayerManager(QObject* parent) : QObject(parent) {}
LayerManager::~LayerManager() = default;

void LayerManager::addLayer(std::unique_ptr<MapLayer> layer)
{
    if (!layer) return;
    MapLayer* ptr = layer.get();
    mLayers.push_back(std::move(layer));
    emit layerAdded(ptr);
}

void LayerManager::removeLayer(const QString& id)
{
    for (int i = 0; i < mLayers.size(); ++i)
    {
        if (mLayers[i]->id() == id)
        {
            mLayers.erase(mLayers.begin() + i);
            emit layerRemoved(id);
            return;
        }
    }
}

void LayerManager::removeLayer(int index)
{
    if (index >= 0 && index < mLayers.size())
    {
        QString id = mLayers[index]->id();
        mLayers.erase(mLayers.begin() + index);
        emit layerRemoved(id);
    }
}

void LayerManager::clear()
{
    mLayers.clear();
    emit layersCleared();
}

MapLayer* LayerManager::layerById(const QString& id) const
{
    for (auto& layer : mLayers)
    {
        if (layer->id() == id)
            return layer.get();
    }
    return nullptr;
}

MapLayer* LayerManager::layerByName(const QString& name) const
{
    for (auto& layer : mLayers)
    {
        if (layer->name() == name)
            return layer.get();
    }
    return nullptr;
}

MapLayer* LayerManager::layerAt(int index) const
{
    if (index >= 0 && index < mLayers.size())
        return mLayers[index].get();
    return nullptr;
}

int LayerManager::layerCount() const
{
    return mLayers.size();
}

QList<MapLayer*> LayerManager::allLayers() const
{
    QList<MapLayer*> result;
    for (auto& layer : mLayers)
        result.append(layer.get());
    return result;
}

QList<VectorLayer*> LayerManager::vectorLayers() const
{
    QList<VectorLayer*> result;
    for (auto& layer : mLayers)
    {
        if (layer->layerType() == MapLayer::VectorLayerType)
            result.append(static_cast<VectorLayer*>(layer.get()));
    }
    return result;
}

QList<RasterLayer*> LayerManager::rasterLayers() const
{
    QList<RasterLayer*> result;
    for (auto& layer : mLayers)
    {
        if (layer->layerType() == MapLayer::RasterLayerType)
            result.append(static_cast<RasterLayer*>(layer.get()));
    }
    return result;
}

void LayerManager::moveLayerUp(int index)
{
    if (index > 0 && index < mLayers.size())
        moveLayer(index, index - 1);
}

void LayerManager::moveLayerDown(int index)
{
    if (index >= 0 && index < mLayers.size() - 1)
        moveLayer(index, index + 1);
}

void LayerManager::moveLayer(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= mLayers.size()) return;
    if (toIndex < 0 || toIndex >= mLayers.size()) return;
    if (fromIndex == toIndex) return;

    auto item = std::move(mLayers[fromIndex]);
    mLayers.erase(mLayers.begin() + fromIndex);
    if (toIndex > fromIndex) toIndex--;
    mLayers.insert(mLayers.begin() + toIndex, std::move(item));
    emit layerOrderChanged();
}

void LayerManager::setLayerVisible(const QString& id, bool visible)
{
    MapLayer* layer = layerById(id);
    if (layer)
        layer->setVisible(visible);
}

MapLayer* LayerManager::findLayerByKeyword(const QString& keyword) const
{
    // 精确匹配
    MapLayer* exact = layerByName(keyword);
    if (exact) return exact;

    // 模糊匹配（包含关键词）
    QString lower = keyword.toLower();
    for (auto& layer : mLayers)
    {
        if (layer->name().toLower().contains(lower))
            return layer.get();
    }

    return nullptr;
}
