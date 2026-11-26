/* FileName: LayerManagerWidget.h
   Author: YourName
   Version: 1.0
   Date: 2026-05-26
   Description: Õº≤„π‹¿Ì∆˜UIøÿº˛
*/

#ifndef LAYERMANAGERWIDGET_H
#define LAYERMANAGERWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>

class LayerManagerWidget : public QWidget
{
    // Q_OBJECT  ◊¢ ÕµÙ£¨±‹√‚MOC“¿¿µ

public:
    explicit LayerManagerWidget(QgsMapCanvas* pCanvas, QWidget* pParent = nullptr);
    ~LayerManagerWidget();

    void addLayer(QgsMapLayer* pLayer);
    void removeLayer(QgsMapLayer* pLayer);
    void refreshLayers();

private:
    void setupUI();
    void updateLayerOrder();

private slots:
    void onItemChanged(QTreeWidgetItem* pItem, int nColumn);
    void onMoveUp();
    void onMoveDown();

private:
    QgsMapCanvas* m_pCanvas;
    QTreeWidget* m_pTreeWidget;
    QPushButton* m_pUpBtn;
    QPushButton* m_pDownBtn;
};

#endif