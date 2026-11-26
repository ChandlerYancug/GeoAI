#pragma once

#include <QWidget>
#include <qgsmapcanvas.h>

class LayerManager;

class OverviewMap : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewMap(QWidget* parent = nullptr);
    ~OverviewMap() override;

    void setLayerManager(LayerManager* manager);
    void syncFromMain(QgsMapCanvas* mainCanvas);

private:
    QgsMapCanvas* mCanvas;
    LayerManager* mLayerManager = nullptr;
};
