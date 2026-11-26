/* FileName: test_vectorlayer.cpp
   Author: YourName
   Version: 1.0
   Date: 2026-05-26
   Description: 测试两个图层 - 建筑物 + 水体
*/

#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDockWidget>
#include <iostream>

#include <qgsapplication.h>
#include <qgsmapcanvas.h>
#include <qgsvectorlayer.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolzoom.h>

#include "LayerManagerWidget.h"

int main(int argc, char* argv[])
{
    QApplication oApp(argc, argv);

    // 初始化QGIS
    QgsApplication::setPrefixPath("D:/QTTT/QGIS-ltr/QGIS-ltr", true);
    QgsApplication::init();
    QgsApplication::initQgis();

    // 创建主窗口
    QMainWindow oMainWin;
    oMainWin.setWindowTitle("Module 1 Test - Two Layers");
    oMainWin.resize(1200, 700);

    // 创建地图画布
    QgsMapCanvas* pCanvas = new QgsMapCanvas(&oMainWin);
    oMainWin.setCentralWidget(pCanvas);

    // 创建图层管理器停靠窗口
    QDockWidget* pDockWidget = new QDockWidget("Layer Manager", &oMainWin);
    LayerManagerWidget* pLayerMgr = new LayerManagerWidget(pCanvas, pDockWidget);
    pDockWidget->setWidget(pLayerMgr);
    oMainWin.addDockWidget(Qt::LeftDockWidgetArea, pDockWidget);

    // 创建工具栏
    QToolBar* pToolBar = new QToolBar("Map Tools");
    oMainWin.addToolBar(pToolBar);

    // ========== 加载第一个图层：建筑物 ==========
    QString sShapePath1 = "D:\\QTTT\\OO_practice_and_task\\oo_FinalProject\\data\\gis_osm_buildings_a_free_1.shp";
    QgsVectorLayer* pLayer1 = new QgsVectorLayer(sShapePath1, "Buildings", "ogr");

    // ========== 加载第二个图层：水体 ==========
    QString sShapePath2 = "D:\\QTTT\\OO_practice_and_task\\oo_FinalProject\\data\\gis_osm_water_a_free_1.shp";
    QgsVectorLayer* pLayer2 = new QgsVectorLayer(sShapePath2, "Water", "ogr");

    QList<QgsMapLayer*> vLayers;
    QgsRectangle oFullExtent;

    // 处理建筑物图层
    if (pLayer1->isValid()) {
        std::cout << "Buildings 加载成功！" << std::endl;
        vLayers.append(pLayer1);
        pLayerMgr->addLayer(pLayer1);
        oFullExtent = pLayer1->extent();
    }
    else {
        std::cout << "Buildings 加载失败，请检查文件路径" << std::endl;
    }

    // 处理水体图层
    if (pLayer2->isValid()) {
        std::cout << "Water 加载成功！" << std::endl;
        vLayers.append(pLayer2);
        pLayerMgr->addLayer(pLayer2);
        oFullExtent.combineExtentWith(pLayer2->extent());
    }
    else {
        std::cout << "Water 加载失败，请检查文件路径" << std::endl;
    }

    // 设置图层到画布
    if (vLayers.size() > 0) {
        pCanvas->setLayers(vLayers);
        pCanvas->setExtent(oFullExtent);
        pCanvas->refresh();
        std::cout << "共加载 " << vLayers.size() << " 个图层" << std::endl;
    }

    // ========== 添加地图工具按钮 ==========
    QAction* pPanAction = new QAction("Pan", &oMainWin);
    pPanAction->setCheckable(true);
    QObject::connect(pPanAction, &QAction::triggered, [=]() {
        pCanvas->setMapTool(new QgsMapToolPan(pCanvas));
        });
    pToolBar->addAction(pPanAction);

    QAction* pZoomAction = new QAction("Zoom", &oMainWin);
    pZoomAction->setCheckable(true);
    QObject::connect(pZoomAction, &QAction::triggered, [=]() {
        pCanvas->setMapTool(new QgsMapToolZoom(pCanvas, false));
        });
    pToolBar->addAction(pZoomAction);

    QAction* pFullExtentAction = new QAction("Full Extent", &oMainWin);
    QObject::connect(pFullExtentAction, &QAction::triggered, [=]() {
        pCanvas->setExtent(oFullExtent);
        pCanvas->refresh();
        });
    pToolBar->addAction(pFullExtentAction);

    QAction* pZoomInAction = new QAction("Zoom In", &oMainWin);
    QObject::connect(pZoomInAction, &QAction::triggered, [=]() {
        pCanvas->zoomIn();
        });
    pToolBar->addAction(pZoomInAction);

    QAction* pZoomOutAction = new QAction("Zoom Out", &oMainWin);
    QObject::connect(pZoomOutAction, &QAction::triggered, [=]() {
        pCanvas->zoomOut();
        });
    pToolBar->addAction(pZoomOutAction);

    pCanvas->setMapTool(new QgsMapToolPan(pCanvas));

    std::cout << "地图漫游功能已启用" << std::endl;
    std::cout << "图层管理器已启用 - 可以调整图层顺序和可见性" << std::endl;

    oMainWin.show();
    int nRet = oApp.exec();

    QgsApplication::exitQgis();
    return nRet;
}