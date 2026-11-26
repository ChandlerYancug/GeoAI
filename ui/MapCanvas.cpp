#include "MapCanvas.h"
#include "../core/MapLayer.h"
#include <QVBoxLayout>
#include <qgsvectorlayer.h>
#include <qgsmaptool.h>
#include <QDebug>

int MapCanvas::sNextId = 1;

MapCanvas::MapCanvas(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    mCanvas = new QgsMapCanvas(this);
    mCanvas->setMinimumSize(400, 300);
    mCanvas->enableAntiAliasing(true);
    mCanvas->setCanvasColor(QColor(245, 245, 245));

    mPanTool = new QgsMapToolPan(mCanvas);
    mZoomInTool = new QgsMapToolZoom(mCanvas, false);
    mZoomOutTool = new QgsMapToolZoom(mCanvas, true);

    layout->addWidget(mCanvas);

    connect(mCanvas, &QgsMapCanvas::xyCoordinates,
            this, &MapCanvas::onCanvasXYCoordinates);

    setMapTool(PanTool);
}

MapCanvas::~MapCanvas() = default;

void MapCanvas::addQgsLayer(QgsMapLayer* layer)
{
    if (!layer) return;
    QList<QgsMapLayer*> layers = mCanvas->layers();
    layers.append(layer);
    mCanvas->setLayers(layers);

    LayerInfo info;
    info.id = QString("layer_%1").arg(sNextId++);
    info.qgsLayer = layer;
    mLayerInfos.append(info);

    if (mCanvas->extent().isEmpty()) {
        mCanvas->setExtent(mCanvas->fullExtent());
    }
    mCanvas->refresh();
    mCanvas->repaint();
}

void MapCanvas::removeQgsLayer(QgsMapLayer* layer)
{
    if (!layer) return;
    QList<QgsMapLayer*> layers = mCanvas->layers();
    layers.removeAll(layer);
    mCanvas->setLayers(layers);
    for (int i = 0; i < mLayerInfos.size(); ++i) {
        if (mLayerInfos[i].qgsLayer == layer) { mLayerInfos.removeAt(i); break; }
    }
    mCanvas->refresh();
    mCanvas->repaint();
}

void MapCanvas::setQgsLayers(const QList<QgsMapLayer*>& layers)
{
    mLayerInfos.clear();
    for (auto* l : layers) {
        LayerInfo info;
        info.id = QString("layer_%1").arg(sNextId++);
        info.qgsLayer = l;
        mLayerInfos.append(info);
    }
    mCanvas->setLayers(layers);
}

void MapCanvas::clearQgsLayers()
{
    mLayerInfos.clear();
    mCanvas->setLayers(QList<QgsMapLayer*>());
    mCanvas->refresh();
}

void MapCanvas::addLayer(MapLayer* layer)
{
    if (!layer) return;
    QgsMapLayer* qgsL = layer->qgsMapLayer();
    if (qgsL) addQgsLayer(qgsL);
}

void MapCanvas::removeLayer(const QString& id)
{
    for (int i = 0; i < mLayerInfos.size(); ++i) {
        if (mLayerInfos[i].id == id) { removeQgsLayer(mLayerInfos[i].qgsLayer); return; }
    }
}

void MapCanvas::setMapTool(MapTool tool)
{
    mCurrentTool = tool;
    switch (tool) {
    case PanTool:     mCanvas->setMapTool(mPanTool); break;
    case ZoomInTool:  mCanvas->setMapTool(mZoomInTool); break;
    case ZoomOutTool: mCanvas->setMapTool(mZoomOutTool); break;
    }
    emit mapToolChanged(tool);
}

void MapCanvas::zoomIn()  { mCanvas->zoomIn(); }
void MapCanvas::zoomOut() { mCanvas->zoomOut(); }
void MapCanvas::zoomToFullExtent() { mCanvas->zoomToFullExtent(); mCanvas->refresh(); }
void MapCanvas::refresh() { mCanvas->refresh(); }

QgsRectangle MapCanvas::currentExtent() const { return mCanvas->extent(); }

void MapCanvas::onCanvasXYCoordinates(const QgsPointXY& pt)
{
    emit mouseGeoCoordinateChanged(pt);
}
