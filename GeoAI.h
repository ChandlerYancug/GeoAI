#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GeoAI.h"
#include <memory>
#include "core/MapLayer.h"
#include <qgspointxy.h>

// 前向声明
class MapCanvas;
class LayerTreeWidget;
class AnalysisPanel;
class NLInputWidget;
class AttributeTableWidget;
class OverviewMap;
class LayerManager;
class QueryParser;
class SpatialAnalyzer;
class AnalysisCommand;
class MapLayer;
class SpatiaLiteDAO;

// ============================================================
// GeoAI — 主窗口（中介者模式）
//   OOP: 中介者模式 — 协调所有UI组件和业务模块
//   通过信号-槽机制实现组件间通信
// ============================================================
class GeoAI : public QMainWindow
{
    Q_OBJECT

public:
    GeoAI(QWidget* parent = nullptr);
    ~GeoAI();

private slots:
    // 文件操作
    void onLoadVectorData();
    void onLoadRasterData();
    void onLoadDatabaseData();

    // SQL查询
    void onSqlQuery();

    // 地图工具
    void onPanTool();
    void onZoomInTool();
    void onZoomOutTool();
    void onZoomFullExtent();

    // 图层操作
    void onLayerMoveUp();
    void onLayerMoveDown();
    void onLayerRemove();

    // 分析操作
    void onExecuteAnalysis();

    // 自然语言查询
    void onNaturalLanguageQuery(const AnalysisCommand& command);

    // 属性表
    void onLayerSelected();

    // 鼠标坐标
    void onMouseCoordinateChanged(const QgsPointXY& coord);

    // 工程存取
    void onSaveProject();
    void onOpenProject();

    // 渲染
    void onSetSimpleRenderer();
    void onToggleRasterGray();

    // 结果保存
    void saveResultToDatabase(VectorLayer* vl);

    // SQL查询辅助
    VectorLayer* executeSqlAndCreateLayer(const QString& dbPath, const QString& sql,
                                           const QString& layerName);

private:
    void setupUI();
    void setupConnections();
    void setupToolBar();
    void setupDockWidgets();
    void initModules();

    // Drag-drop file loading
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // Map screenshot
    void onExportScreenshot();
    void onAbout();

    // —— 核心模块 ——
    std::unique_ptr<LayerManager> mLayerManager;
    std::unique_ptr<QueryParser> mQueryParser;

    // —— UI组件 ——
    MapCanvas* mMapCanvas = nullptr;
    LayerTreeWidget* mLayerTree = nullptr;
    AnalysisPanel* mAnalysisPanel = nullptr;
    NLInputWidget* mNLInput = nullptr;
    AttributeTableWidget* mAttributeTable = nullptr;
    OverviewMap* mOverviewMap = nullptr;

    // —— 状态 ——
    Ui::GeoAIClass ui;
};
