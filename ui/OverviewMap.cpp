#include "OverviewMap.h"
#include "../core/MapLayer.h"
#include "../core/LayerManager.h"
#include <QVBoxLayout>

OverviewMap::OverviewMap(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    mCanvas = new QgsMapCanvas(this);
    mCanvas->setMinimumSize(180, 120);
    mCanvas->setMaximumHeight(180);
    mCanvas->enableAntiAliasing(true);
    mCanvas->setCanvasColor(QColor(250, 250, 250));
    mCanvas->setInteractive(false); // read-only

    layout->addWidget(mCanvas);
}

OverviewMap::~OverviewMap() = default;

void OverviewMap::setLayerManager(LayerManager* manager)
{
    mLayerManager = manager;
}

void OverviewMap::syncFromMain(QgsMapCanvas* mainCanvas)
{
    if (!mainCanvas || !mLayerManager) return;

    // Copy layers from main canvas
    QList<QgsMapLayer*> layers = mainCanvas->layers();
    mCanvas->setLayers(layers);

    // Always show full extent
    mCanvas->setExtent(mainCanvas->fullExtent());
    mCanvas->refresh();
}
