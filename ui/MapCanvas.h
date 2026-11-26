#pragma once

#include <QWidget>
#include <qgsmapcanvas.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolzoom.h>
#include <qgspointxy.h>
#include <qgsrectangle.h>
#include <QList>

class MapLayer;
class QgsMapLayer;

// ============================================================
// MapCanvas — wrapper around QgsMapCanvas (QGIS native canvas)
//   Provides: pan, zoom, box-zoom, coordinate tracking, draw-point
// ============================================================
class MapCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit MapCanvas(QWidget* parent = nullptr);
    ~MapCanvas() override;

    // Access the underlying QgsMapCanvas
    QgsMapCanvas* qgsCanvas() const { return mCanvas; }

    // Add layers directly to QGIS canvas
    void addQgsLayer(QgsMapLayer* layer);
    void removeQgsLayer(QgsMapLayer* layer);
    void setQgsLayers(const QList<QgsMapLayer*>& layers);
    void clearQgsLayers();

    // Also accept MapLayer* (extracts QgsMapLayer*)
    void addLayer(MapLayer* layer);
    void removeLayer(const QString& id);

    // Navigation
    enum MapTool { PanTool, ZoomInTool, ZoomOutTool };
    void setMapTool(MapTool tool);
    MapTool currentTool() const { return mCurrentTool; }

    void zoomIn();
    void zoomOut();
    void zoomToFullExtent();
    void refresh();

    QgsRectangle currentExtent() const;

signals:
    void mapToolChanged(MapTool tool);
    void mouseGeoCoordinateChanged(const QgsPointXY& geoCoord);

private slots:
    void onCanvasXYCoordinates(const QgsPointXY& pt);

private:
    QgsMapCanvas* mCanvas;
    QgsMapToolPan* mPanTool;
    QgsMapToolZoom* mZoomInTool;
    QgsMapToolZoom* mZoomOutTool;
    MapTool mCurrentTool = PanTool;

    struct LayerInfo {
        QString id;
        QgsMapLayer* qgsLayer = nullptr;
    };
    QList<LayerInfo> mLayerInfos;

    static int sNextId;
};
