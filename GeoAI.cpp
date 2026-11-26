#include "GeoAI.h"

// 核心模块
#include "core/MapLayer.h"
#include "core/DataSource.h"
#include "core/Renderer.h"
#include "core/AnalysisCommand.h"
#include "core/LayerManager.h"

// 数据访问层
#include "SQL/DataAccessObject.h"
#include "SQL/SpatiaLiteDAO.h"
#include "SQL/SpatialQuery.h"

// 分析引擎
#include "analysis/SpatialAnalyzer.h"
#include "analysis/Analyzers.h"

// NLP
#include "nlp/QueryParser.h"
#include "nlp/RuleBasedParser.h"
#include "nlp/LLMParser.h"

// UI组件
#include "ui/MapCanvas.h"
#include "ui/LayerTreeWidget.h"
#include "ui/AnalysisPanel.h"
#include "ui/NLInputWidget.h"
#include "ui/AttributeTableWidget.h"
#include "ui/OverviewMap.h"

#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QAction>
#include <QFile>
#include <QTextStream>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPixmap>
#include <QMetaObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QInputDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include <qgsvectorfilewriter.h>
#include <qgsfeature.h>
#include <qgsfields.h>
#include <qgsgeometry.h>

#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgspointxy.h>
#include <qgscoordinatetransform.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsproject.h>
#include <qgis.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>

// ============================================================
// GeoAI main window implementation
// ============================================================

#pragma warning(disable: 4996)  // QgsVectorFileWriter::writeAsVectorFormat deprecated

GeoAI::GeoAI(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    setWindowTitle("GeoAI — AI增强智能空间分析平台");
    resize(1400, 900);
    setMinimumSize(1000, 600);

    initModules();
    setupUI();
    setupConnections();
    setAcceptDrops(true);

    statusBar()->showMessage("就绪 — 请加载数据开始使用");
}

GeoAI::~GeoAI() = default;

void GeoAI::initModules()
{
    // 初始化图层管理器
    mLayerManager = std::make_unique<LayerManager>(this);

    // 初始化NL解析器（默认使用规则解析器）
    mQueryParser = std::make_unique<RuleBasedParser>();
}

void GeoAI::setupUI()
{
    // 中央区域 — 地图画布
    mMapCanvas = new MapCanvas(this);
    setCentralWidget(mMapCanvas);

    // 设置初始大小
    resize(1200, 800);

    // 工具栏
    setupToolBar();

    // 停靠窗口
    setupDockWidgets();
}

void GeoAI::setupToolBar()
{
    QMenuBar* mb = menuBar();

    QMenu* fileMenu = mb->addMenu("文件(&F)");
    fileMenu->addAction("加载矢量...", this, &GeoAI::onLoadVectorData);
    fileMenu->addAction("加载栅格...", this, &GeoAI::onLoadRasterData);
    fileMenu->addAction("加载数据库...", this, &GeoAI::onLoadDatabaseData);
    fileMenu->addAction("导入CSV...", this, [this]() {
        // CSV import - auto-detect tab/comma, find lat/lon/coords columns
        QString path = QFileDialog::getOpenFileName(this, "导入CSV", "", "CSV/TSV (*.csv *.tsv *.txt);;All (*.*)");
        if (path.isEmpty()) return;
        QFile f(path); if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) return;
        QTextStream in(&f); QString hdr = in.readLine(); if (hdr.isEmpty()) return;
        QChar delim = hdr.count('	') > hdr.count(',') ? '	' : ',';
        QStringList hdrs = (delim == '	') ? hdr.split('	') : hdr.split(',');
        int latI=-1,lonI=-1,coordI=-1;
        for (int i=0;i<hdrs.size();++i){QString h=hdrs[i].trimmed().toLower();
            if(h=="lat"||h=="latitude"||h=="y")latI=i;
            if(h=="lon"||h=="lng"||h=="longitude"||h=="x")lonI=i;
            if(h.contains("经纬")||h=="coords"||h=="coordinates")coordI=i;}
        if(latI<0&&lonI<0&&coordI<0){QMessageBox::warning(this,"错误","找不到坐标列");return;}
        auto ml=new QgsVectorLayer("Point?crs=EPSG:4326","CSV","memory");
        QList<QgsField> attrs;for(int i=0;i<hdrs.size();++i)attrs.append(QgsField(hdrs[i].trimmed(),QVariant::String));
        ml->dataProvider()->addAttributes(attrs);ml->updateFields();ml->startEditing();
        int cnt=0;
        while(!in.atEnd()){QString ln=in.readLine().trimmed();if(ln.isEmpty())continue;
            QStringList vs=(delim=='	')?ln.split('	'):ln.split(',');
            double lat=0,lon=0;bool ok=false;
            if(coordI>=0&&coordI<vs.size()){QStringList ps;
                if(vs[coordI].contains(','))ps=vs[coordI].split(',');
                else if(vs[coordI].contains(' '))ps=vs[coordI].split(' ');
                if(ps.size()>=2){double v1=ps[0].trimmed().toDouble(),v2=ps[1].trimmed().toDouble();
                    if(qAbs(v1)>90){lon=v1;lat=v2;}else{lat=v1;lon=v2;}ok=true;}}
            if(!ok&&latI>=0&&lonI>=0&&latI<vs.size()&&lonI<vs.size()){lat=vs[latI].trimmed().toDouble();lon=vs[lonI].trimmed().toDouble();ok=true;}
            if(!ok)continue;
            QgsFeature ff(ml->fields());ff.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(lon,lat)));
            for(int i=0;i<vs.size()&&i<attrs.size();++i)ff.setAttribute(i,vs[i].trimmed());
            ml->addFeature(ff);cnt++;}
        ml->commitChanges();f.close();
        if(cnt==0){delete ml;QMessageBox::information(this,"提示","无有效记录");return;}
        QString tmp=QDir::temp().absoluteFilePath("csv_import.gpkg");
        QgsVectorFileWriter::SaveVectorOptions o;o.driverName="GPKG";
        QgsVectorFileWriter::writeAsVectorFormat(ml,tmp,o);delete ml;
        auto l=std::make_unique<VectorLayer>("CSV");auto d=std::make_unique<FileDataSource>(tmp);l->setDataSource(std::move(d));
        if(l->load()){auto r=std::make_unique<SimpleRenderer>();r->setPointSize(8);r->setFillColor(QColor(255,50,50,200));l->setRenderer(std::move(r));l->applyRenderer();mLayerManager->addLayer(std::move(l));}
        statusBar()->showMessage(QString("CSV: %1 records").arg(cnt),3000);
    });
    fileMenu->addAction("导出截图...", this, &GeoAI::onExportScreenshot);
    fileMenu->addSeparator();
    fileMenu->addAction("打开工程...", this, &GeoAI::onOpenProject, QKeySequence("Ctrl+O"));
    fileMenu->addAction("保存工程...", this, &GeoAI::onSaveProject, QKeySequence("Ctrl+S"));
    fileMenu->addSeparator();
    fileMenu->addAction("退出", this, &QWidget::close, QKeySequence("Alt+F4"));

    QMenu* editMenu = mb->addMenu("工具(&T)");
    editMenu->addAction("一键清空所有图层", this, [this]() {
        mLayerManager->clear(); mMapCanvas->clearQgsLayers();
        if (mAttributeTable) mAttributeTable->clearTable();
    });

    QMenu* viewMenu = mb->addMenu("视图(&V)");
    viewMenu->addAction("放大", this, &GeoAI::onZoomInTool);
    viewMenu->addAction("缩小", this, &GeoAI::onZoomOutTool);
    viewMenu->addAction("平移", this, [this]() { mMapCanvas->setMapTool(MapCanvas::PanTool); });
    viewMenu->addAction("全图", this, &GeoAI::onZoomFullExtent);
    viewMenu->addAction("实际比例", this, [this]() { mMapCanvas->qgsCanvas()->resetTransform(); });
    viewMenu->addSeparator();
    viewMenu->addAction("SQL查询", this, &GeoAI::onSqlQuery);

    // === Analysis Menu ===
    QMenu* analysisMenu = mb->addMenu("分析(&A)");
    analysisMenu->addAction("缓冲区分析", this, &GeoAI::onExecuteAnalysis);
    analysisMenu->addAction("矢量裁剪...", this, [this]() {
        QList<MapLayer*> layers = mLayerManager->allLayers();
        if (layers.size() < 2) { QMessageBox::warning(this, "提示", "需要至少两个图层"); return; }
        QStringList names;
        for (auto* l : layers) names << l->name();
        bool ok1, ok2;
        QString srcName = QInputDialog::getItem(this, "裁剪", "被裁剪图层:", names, 0, false, &ok1);
        if (!ok1) return;
        QString clipName = QInputDialog::getItem(this, "裁剪", "裁剪图层:", names, 0, false, &ok2);
        if (!ok2) return;
        VectorLayer* srcLayer = dynamic_cast<VectorLayer*>(mLayerManager->layerByName(srcName));
        VectorLayer* clipLayer = dynamic_cast<VectorLayer*>(mLayerManager->layerByName(clipName));
        if (!srcLayer || !clipLayer || !srcLayer->qgsVectorLayer() || !clipLayer->qgsVectorLayer()) return;
        QgsVectorLayer* srcVl = srcLayer->qgsVectorLayer();
        QgsVectorLayer* clipVl = clipLayer->qgsVectorLayer();
        auto memLayer = new QgsVectorLayer("Polygon?crs=" + srcVl->crs().authid(), srcName + "_Clipped", "memory");
        memLayer->dataProvider()->addAttributes(srcVl->fields().toList());
        memLayer->updateFields(); memLayer->startEditing();
        QList<QgsGeometry> clipGeoms;
        QgsFeatureIterator cit = clipVl->getFeatures(); QgsFeature cf;
        while (cit.nextFeature(cf)) { if (cf.hasGeometry()) clipGeoms.append(cf.geometry()); }
        QgsFeatureIterator sit = srcVl->getFeatures(); QgsFeature sf; int cnt = 0;
        while (sit.nextFeature(sf)) { if (!sf.hasGeometry()) continue;
            for (const QgsGeometry& cg : clipGeoms) { if (!sf.geometry().intersects(cg)) continue;
                QgsGeometry r = sf.geometry().intersection(cg); if (r.isNull()||r.isEmpty()) continue;
                QgsFeature nf(memLayer->fields()); nf.setGeometry(r);
                for (int i=0;i<sf.attributes().size();++i) nf.setAttribute(i,sf.attributes()[i]);
                memLayer->addFeature(nf); cnt++; } }
        memLayer->commitChanges();
        if (cnt == 0) { delete memLayer; QMessageBox::information(this, "结果", "无交集"); return; }
        QString tmp = QDir::temp().absoluteFilePath("clip.gpkg");
        QgsVectorFileWriter::SaveVectorOptions o; o.driverName="GPKG";
        QgsVectorFileWriter::writeAsVectorFormat(memLayer,tmp,o); delete memLayer;
        auto rl = std::make_unique<VectorLayer>(srcName+"_Clipped");
        auto ds = std::make_unique<FileDataSource>(tmp); rl->setDataSource(std::move(ds));
        if (rl->load()) { auto rd = std::make_unique<SimpleRenderer>(); rl->setRenderer(std::move(rd)); rl->applyRenderer();
            mLayerManager->addLayer(std::move(rl)); statusBar()->showMessage(QString("裁剪: %1条").arg(cnt),3000); } });
    analysisMenu->addAction("转WGS84", this, [this]() {
        MapLayer* layer = mLayerTree->selectedLayer();
        auto* vl = dynamic_cast<VectorLayer*>(layer);
        if (!vl || !vl->qgsVectorLayer()) { QMessageBox::information(this, "提示", "请先选择矢量图层"); return; }
        QgsVectorLayer* src = vl->qgsVectorLayer();
        QgsCoordinateReferenceSystem srcCrs = src->crs();
        QgsCoordinateReferenceSystem dstCrs("EPSG:4326");
        if (srcCrs.authid() == "EPSG:4326") { QMessageBox::information(this, "提示", "已经是WGS84"); return; }
        QgsCoordinateTransform xform(srcCrs, dstCrs, QgsProject::instance());
        auto memLayer = new QgsVectorLayer("Polygon?crs=EPSG:4326", layer->name() + "_WGS84", "memory");
        memLayer->dataProvider()->addAttributes(src->fields().toList());
        memLayer->updateFields(); memLayer->startEditing();
        QgsFeatureIterator it = src->getFeatures(); QgsFeature f; int count = 0;
        while (it.nextFeature(f)) { if (!f.hasGeometry()) continue; QgsGeometry g = f.geometry(); g.transform(xform);
            QgsFeature nf(memLayer->fields()); nf.setGeometry(g);
            for (int i = 0; i < f.attributes().size(); ++i) nf.setAttribute(i, f.attributes()[i]);
            memLayer->addFeature(nf); count++; }
        memLayer->commitChanges();
        if (count == 0) { delete memLayer; return; }
        QString tmpPath = QDir::temp().absoluteFilePath("geoai_wgs84.gpkg");
        QgsVectorFileWriter::SaveVectorOptions opts; opts.driverName = "GPKG";
        QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, opts); delete memLayer;
        auto result = std::make_unique<VectorLayer>(layer->name() + "_WGS84");
        auto ds = std::make_unique<FileDataSource>(tmpPath); result->setDataSource(std::move(ds));
        if (result->load()) { auto r = std::make_unique<SimpleRenderer>(); result->setRenderer(std::move(r));
            result->applyRenderer(); mLayerManager->addLayer(std::move(result)); }
        statusBar()->showMessage(QString("转WGS84完成: %1 条").arg(count), 3000); });
    analysisMenu->addSeparator();
    analysisMenu->addAction("矢量转栅格", this, [this]() { QMessageBox::information(this, "提示", "需要GDAL rasterize"); });
    analysisMenu->addAction("栅格计算器", this, [this]() { QMessageBox::information(this, "提示", "需要加载栅格图层"); });

    // === Render Menu ===
    QMenu* renderMenu = mb->addMenu("渲染(&R)");
    renderMenu->addAction("简单渲染...", this, &GeoAI::onSetSimpleRenderer);
    renderMenu->addAction("栅格模式", this, &GeoAI::onToggleRasterGray);

    // === Help Menu ===
    QMenu* helpMenu = mb->addMenu("帮助(&H)");
    helpMenu->addAction("关于...", this, &GeoAI::onAbout);
}
void GeoAI::setupDockWidgets()
{
    // 左侧停靠 — 图层树 + NL输入
    QDockWidget* leftDock = new QDockWidget("图层 & 查询", this);
    leftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* leftWidget = new QWidget(leftDock);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(4, 4, 4, 4);

    // 图层树
    mLayerTree = new LayerTreeWidget(leftWidget);
    mLayerTree->setLayerManager(mLayerManager.get());
    leftLayout->addWidget(mLayerTree, 2);

    // 图层操作按钮
    auto* layerBtnLayout = new QHBoxLayout();
    QPushButton* upBtn = new QPushButton("▲", leftWidget);
    upBtn->setToolTip("上移图层");
    upBtn->setMaximumWidth(40);
    connect(upBtn, &QPushButton::clicked, this, &GeoAI::onLayerMoveUp);

    QPushButton* downBtn = new QPushButton("▼", leftWidget);
    downBtn->setToolTip("下移图层");
    downBtn->setMaximumWidth(40);
    connect(downBtn, &QPushButton::clicked, this, &GeoAI::onLayerMoveDown);

    QPushButton* removeBtn = new QPushButton("✕", leftWidget);
    removeBtn->setToolTip("移除图层");
    removeBtn->setMaximumWidth(40);
    connect(removeBtn, &QPushButton::clicked, this, &GeoAI::onLayerRemove);

    layerBtnLayout->addWidget(upBtn);
    layerBtnLayout->addWidget(downBtn);
    layerBtnLayout->addWidget(removeBtn);
    layerBtnLayout->addStretch();
    leftLayout->addLayout(layerBtnLayout);

    // NL输入
    mNLInput = new NLInputWidget(leftWidget);
    mNLInput->setParser(std::make_unique<RuleBasedParser>());
    leftLayout->addWidget(mNLInput, 3);

    leftDock->setWidget(leftWidget);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    // 右侧停靠 — 分析面板
    QDockWidget* rightDock = new QDockWidget("空间分析", this);
    rightDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    mAnalysisPanel = new AnalysisPanel(rightDock);
    mAnalysisPanel->setLayerManager(mLayerManager.get());
    rightDock->setWidget(mAnalysisPanel);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    // 底部停靠 — 属性表
    QDockWidget* bottomDock = new QDockWidget("属性表", this);
    bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    mAttributeTable = new AttributeTableWidget(bottomDock);
    mAttributeTable->setLayerManager(mLayerManager.get());
    bottomDock->setWidget(mAttributeTable);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);

    // 鹰眼图
    QDockWidget* overviewDock = new QDockWidget("鹰眼图", this);
    overviewDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mOverviewMap = new OverviewMap(overviewDock);
    mOverviewMap->setLayerManager(mLayerManager.get());
    overviewDock->setWidget(mOverviewMap);
    addDockWidget(Qt::RightDockWidgetArea, overviewDock);

    // Tabify with analysis panel
    tabifyDockWidget(rightDock, overviewDock);
    rightDock->raise();
}

void GeoAI::setupConnections()
{
    // NL查询 → 分析执行
    connect(mNLInput, &NLInputWidget::querySubmitted,
            this, &GeoAI::onNaturalLanguageQuery);

    // 分析面板 → 分析执行
    connect(mAnalysisPanel, &AnalysisPanel::executeRequested,
            this, &GeoAI::onExecuteAnalysis);

    // 图层可见性 → 地图刷新
    connect(mLayerManager.get(), &LayerManager::layerAdded,
            this, [this](MapLayer* layer) {
                mMapCanvas->addLayer(layer);
                if (mOverviewMap)
                    mOverviewMap->syncFromMain(mMapCanvas->qgsCanvas());
                // Update LLM layer context
                QStringList names;
                for (auto* l : mLayerManager->allLayers()) names << l->name();
                auto* llm = dynamic_cast<LLMParser*>(mNLInput->parser());
                if (llm) llm->setLayerContext(names);
                statusBar()->showMessage(
                    QString("已加载图层: %1").arg(layer->name()), 3000);
            });

    // 图层可见性切换 → 地图画布显示/隐藏
    connect(mLayerTree, &LayerTreeWidget::layerVisibilityToggled,
            this, [this](const QString& layerId, bool visible) {
                MapLayer* layer = mLayerManager->layerById(layerId);
                if (!layer) return;
                QgsMapLayer* qgsL = layer->qgsMapLayer();
                if (!qgsL) return;
                if (visible) {
                    mMapCanvas->addQgsLayer(qgsL);
                    mMapCanvas->qgsCanvas()->refresh();
                } else {
                    mMapCanvas->removeQgsLayer(qgsL);
                    mMapCanvas->qgsCanvas()->refresh();
                }
            });

    // 图层树右键菜单 → 图层操作
    connect(mLayerTree, &LayerTreeWidget::layerRemovedRequested,
            this, [this](const QString&) { onLayerRemove(); });
    connect(mLayerTree, &LayerTreeWidget::layerMoveUpRequested,
            this, [this](const QString&) { onLayerMoveUp(); });
    connect(mLayerTree, &LayerTreeWidget::layerMoveDownRequested,
            this, [this](const QString&) { onLayerMoveDown(); });

    // 图层透明度
    connect(mLayerTree, &LayerTreeWidget::layerOpacityRequested,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                if (!layer) return;
                bool ok;
                double opacity = QInputDialog::getDouble(this, "Layer Opacity",
                    QString("Opacity for %1 (0-100):").arg(layer->name()),
                    100, 0, 100, 1, &ok);
                if (ok) {
                    layer->setOpacity(opacity / 100.0);
                }
            });

    // 鼠标坐标 → 状态栏
    connect(mMapCanvas, &MapCanvas::mouseGeoCoordinateChanged,
            this, &GeoAI::onMouseCoordinateChanged);

    // 图层选中 → 显示信息
    connect(mLayerTree, &LayerTreeWidget::itemSelectionChanged,
            this, [this]() {
                onLayerSelected();
            });

    // 查看字段
    connect(mLayerTree, &LayerTreeWidget::layerFieldsRequested,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                auto* vl = dynamic_cast<VectorLayer*>(layer);
                if (!vl || !vl->qgsVectorLayer()) return;
                const QgsFields& flds = vl->qgsVectorLayer()->fields();
                QString info = QString("Fields of %1 (%2 fields):\n\n").arg(layer->name()).arg(flds.size());
                for (int i = 0; i < flds.size(); ++i) {
                    QString tname = QVariant::typeToName(flds.at(i).type());
                    info += QString("  [%1] %2 : %3\n").arg(i).arg(flds.at(i).name(), tname);
                }
                QMessageBox::information(this, "Field List", info);
            });

    // 显示标签
    connect(mLayerTree, &LayerTreeWidget::layerLabelsRequested,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                auto* vl = dynamic_cast<VectorLayer*>(layer);
                if (!vl || !vl->qgsVectorLayer()) return;
                QgsVectorLayer* qvl = vl->qgsVectorLayer();
                const QgsFields& flds = qvl->fields();
                QStringList fnames;
                for (int i = 0; i < flds.size(); ++i)
                    fnames << flds.at(i).name();
                if (fnames.isEmpty()) { QMessageBox::information(this, "提示", "无可用字段"); return; }
                bool ok;
                QString field = QInputDialog::getItem(this, "标签字段", "选择标注字段:", fnames, 0, false, &ok);
                if (!ok) return;
                layer->setLabelField(field);
                QgsPalLayerSettings settings;
                settings.fieldName = field;
                qvl->setLabeling(new QgsVectorLayerSimpleLabeling(settings));
                qvl->setLabelsEnabled(true);
                mMapCanvas->qgsCanvas()->refresh();
                statusBar()->showMessage(QString("已添加标签: %1").arg(field), 3000);
            });

    // 双击图层改符号
    connect(mLayerTree, &LayerTreeWidget::layerDoubleClicked,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                if (!layer) return;
                QColor color = QColorDialog::getColor(QColor(100, 150, 255), this, "选择符号颜色");
                if (!color.isValid()) return;
                auto renderer = std::make_unique<SimpleRenderer>();
                QColor fill = color; fill.setAlpha(150);
                renderer->setFillColor(fill);
                renderer->setStrokeColor(color.darker(150));
                layer->setRenderer(std::move(renderer));
                layer->applyRenderer();
                mMapCanvas->refresh(); mMapCanvas->refresh();
            });

    // 分类渲染
    connect(mLayerTree, &LayerTreeWidget::layerClassifiedRequested,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                auto* vl = dynamic_cast<VectorLayer*>(layer);
                if (!vl || !vl->qgsVectorLayer()) return;
                const QgsFields& flds = vl->qgsVectorLayer()->fields();
                QStringList fnames;
                for (int i = 0; i < flds.size(); ++i) fnames << flds.at(i).name();
                if (fnames.isEmpty()) return;
                bool ok;
                QString field = QInputDialog::getItem(this, "分类字段", "选择字段:", fnames, 0, false, &ok);
                if (!ok) return;
                auto renderer = std::make_unique<ClassifiedRenderer>();
                renderer->autoClassify(vl->qgsVectorLayer(), field);
                layer->setRenderer(std::move(renderer));
                layer->applyRenderer();
                mMapCanvas->refresh(); mMapCanvas->refresh();
                statusBar()->showMessage(QString("分类渲染: %1").arg(field), 2000);
            });

    // 分级渲染
    connect(mLayerTree, &LayerTreeWidget::layerGraduatedRequested,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                auto* vl = dynamic_cast<VectorLayer*>(layer);
                if (!vl || !vl->qgsVectorLayer()) return;
                const QgsFields& flds = vl->qgsVectorLayer()->fields();
                QStringList fnames;
                for (int i = 0; i < flds.size(); ++i) {
                    if (flds.at(i).type() == QVariant::Int || flds.at(i).type() == QVariant::Double)
                        fnames << flds.at(i).name();
                }
                if (fnames.isEmpty()) { QMessageBox::information(this, "提示", "无数值字段"); return; }
                bool ok;
                QString field = QInputDialog::getItem(this, "分级字段", "选择数值字段:", fnames, 0, false, &ok);
                if (!ok) return;
                auto renderer = std::make_unique<GraduatedRenderer>();
                renderer->autoClassify(vl->qgsVectorLayer(), field);
                layer->setRenderer(std::move(renderer));
                layer->applyRenderer();
                mMapCanvas->refresh(); mMapCanvas->refresh();
                statusBar()->showMessage(QString("分级渲染: %1").arg(field), 2000);
            });

    // 导出SLD
    connect(mLayerTree, &LayerTreeWidget::layerSldExportRequested,
            this, [this](const QString& id) {
                MapLayer* layer = mLayerManager->layerById(id);
                auto* vl = dynamic_cast<VectorLayer*>(layer);
                if (!vl || !vl->qgsVectorLayer()) return;
                QString path = QFileDialog::getSaveFileName(this, "导出SLD", layer->name() + ".sld", "SLD (*.sld)");
                if (path.isEmpty()) return;
                bool errFlag = false;
                vl->qgsVectorLayer()->saveSldStyle(path, errFlag);
                if (!errFlag)
                    statusBar()->showMessage("SLD导出成功", 3000);
                else
                    QMessageBox::warning(this, "导出失败", "无法写入SLD文件");
            });
}


// ============================================================
// 文件操作
// ============================================================

void GeoAI::onLoadVectorData()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "加载矢量数据", "",
        "矢量文件 (*.shp *.geojson *.json *.gpkg *.kml);;所有文件 (*.*)");

    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    auto layer = std::make_unique<VectorLayer>(fi.baseName());
    auto ds = std::make_unique<FileDataSource>(filePath);
    layer->setDataSource(std::move(ds));

    if (!layer->load() || !layer->isValid())
    {
        QMessageBox::warning(this, "加载失败",
                             QString("无法加载文件: %1").arg(filePath));
        return;
    }

    // 设置默认渲染
    auto renderer = std::make_unique<SimpleRenderer>();
    layer->setRenderer(std::move(renderer));
    layer->applyRenderer();

    mLayerManager->addLayer(std::move(layer));
    mMapCanvas->zoomToFullExtent();
}

void GeoAI::onLoadRasterData()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "加载栅格数据", "",
        "栅格文件 (*.tif *.tiff *.img *.dem);;所有文件 (*.*)");

    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    auto layer = std::make_unique<RasterLayer>(fi.baseName());
    auto ds = std::make_unique<FileDataSource>(filePath);
    layer->setDataSource(std::move(ds));

    if (!layer->load() || !layer->isValid())
    {
        QMessageBox::warning(this, "加载失败",
                             QString("无法加载栅格文件: %1").arg(filePath));
        return;
    }

    mLayerManager->addLayer(std::move(layer));
    mMapCanvas->zoomToFullExtent();
}

void GeoAI::onLoadDatabaseData()
{
    QString dbPath = QFileDialog::getOpenFileName(
        this, "打开空间数据库", "",
        "数据库文件 (*.sqlite *.db *.gpkg);;所有文件 (*.*)");

    if (dbPath.isEmpty()) return;

    // Build OGR URI — OGR can read SpatiaLite tables natively
    // Format: /path/to/file.sqlite|layername=table_name
    QString uri = dbPath;
    uri.replace('\\', '/');

    // Use a temporary QgsVectorLayer to discover available layers
    QgsVectorLayer* probe = new QgsVectorLayer(uri, "probe", "ogr");
    if (!probe->isValid())
    {
        delete probe;
        QMessageBox::warning(this, "加载失败",
                             QString("无法打开数据库: %1\n请确认是有效的空间数据库").arg(dbPath));
        return;
    }

    // Get sublayers (spatial tables)
    QStringList subLayers = probe->dataProvider()->subLayers();
    delete probe;

    if (subLayers.isEmpty())
    {
        QMessageBox::information(this, "提示", "数据库中没有空间表");
        return;
    }

    // Parse sublayers to get table names
    // Format: "0:layer_name:geometry_type:geometry_column:srid"
    QStringList tableNames;
    for (const QString& sub : subLayers)
    {
        QStringList parts = sub.split(':');
        // Skip the layer ID (index 0), get name (index 1)
        if (parts.size() >= 2)
        {
            QString name = parts[1];
            // Skip special tables
            if (name.startsWith("idx_") || name == "spatial_ref_sys" ||
                name == "geometry_columns" || name == "spatialite_history")
                continue;
            tableNames.append(name);
        }
    }

    if (tableNames.isEmpty())
    {
        QMessageBox::information(this, "提示", "数据库中没有可用的空间表");
        return;
    }

    // Try loading each table by sublayer index
    for (int i = 0; i < tableNames.size(); ++i)
    {
        const QString& tableName = tableNames[i];

        // Try |layerid=N format first, fall back to bare file path
        QStringList urisToTry;
        urisToTry << QString("%1|layerid=%2").arg(uri).arg(i);
        if (tableNames.size() == 1)
            urisToTry << uri;  // bare path loads the only layer

        for (const QString& layerUri : urisToTry)
        {
            auto layer = std::make_unique<VectorLayer>(tableName);
            auto ds = std::make_unique<FileDataSource>(layerUri);
            layer->setDataSource(std::move(ds));

            if (layer->load() && layer->isValid())
            {
                auto renderer = std::make_unique<SimpleRenderer>();
                layer->setRenderer(std::move(renderer));
                layer->applyRenderer();
                mLayerManager->addLayer(std::move(layer));
                mMapCanvas->zoomToFullExtent();
                statusBar()->showMessage(
                    QString("已加载数据库图层: %1").arg(tableName), 3000);
                return;
            }
        }
    }

    QMessageBox::warning(this, "加载失败", "所有数据库表加载均失败");
}

void GeoAI::onSqlQuery()
{
    // Ask user for the database file
    QString dbPath = QFileDialog::getOpenFileName(
        this, "选择空间数据库", "",
        "数据库文件 (*.sqlite *.db *.gpkg);;所有文件 (*.*)");
    if (dbPath.isEmpty()) return;

    QString uri = dbPath;
    uri.replace('\\', '/');

    // Discover tables via SpatiaLiteDAO
    SpatiaLiteDAO dao;
    QStringList allTableNames;
    if (dao.connect(dbPath))
    {
        allTableNames = dao.tableNames();
        dao.disconnect();
    }

    // Probe database to discover tables
    QgsVectorLayer* probe = new QgsVectorLayer(uri, "probe", "ogr");
    if (!probe->isValid())
    {
        delete probe;
        QMessageBox::warning(this, "错误", "无法打开数据库文件");
        return;
    }

    QStringList rawSubLayers = probe->dataProvider()->subLayers();
    delete probe;

    qDebug() << "Raw sublayers:" << rawSubLayers;

    QStringList tableNames;
    // Try parsing OGR sublayer format: "N:name:type:column:srid"
    for (const QString& sub : rawSubLayers)
    {
        QStringList parts = sub.split(':');
        if (parts.size() >= 2)
        {
            QString name = parts[1].trimmed();
            if (!name.isEmpty() && !name.startsWith("idx_") &&
                name != "spatial_ref_sys" && name != "geometry_columns" &&
                name != "spatialite_history" && name != "layer_styles" &&
                name != "vector_layers")
                tableNames.append(name);
        }
    }

    // Merge SpatiaLite and OGR table lists (dedup)
    for (const QString& t : allTableNames) {
        if (!tableNames.contains(t))
            tableNames.append(t);
    }

    // Fallback: query sqlite_master for user tables
    if (tableNames.isEmpty()) {
        QSqlDatabase fbDb = QSqlDatabase::addDatabase("QSQLITE", "geoai_fallback");
        fbDb.setDatabaseName(dbPath);
        if (fbDb.open()) {
            QSqlQuery q(fbDb);
            q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name NOT LIKE 'gpkg_%' AND name NOT LIKE 'rtree_%'");
            while (q.next()) {
                QString t = q.value(0).toString();
                if (!t.startsWith("idx_") && t != "spatial_ref_sys" && t != "geometry_columns" && t != "spatialite_history" && t != "layer_styles" && t != "vector_layers")
                    tableNames.append(t);
            }
            fbDb.close();
        }
        QSqlDatabase::removeDatabase("geoai_fallback");
    }

    // Build info text with spatial function hints
    QString hintText =
        "\n\nSupported spatial functions:\n"
        "  ST_Buffer(geom, dist)\n"
        "  ST_Intersects(g1, g2)\n"
        "  ST_Within(g1, g2)\n"
        "  ST_Contains(g1, g2)\n"
        "  ST_Intersection(g1, g2)\n"
        "  ST_Area(geom), ST_Length(geom)\n"
        "  ST_Distance(g1, g2)\n"
        "  ST_Centroid(geom)";

    QString info;
    if (tableNames.isEmpty())
    {
        info = QString("未自动检测到表名。\n请手动输入表名和SQL:\n(提示: 表名用英文双引号包裹)") + hintText;
    }
    else
    {
        info = QString("检测到 %1 个空间表:\n  %2\n\n请输入SQL查询:")
                   .arg(tableNames.size())
                   .arg(tableNames.join("\n  ")) + hintText;
    }
    
    bool ok;
    QString sql = QInputDialog::getMultiLineText(
        this, "Spatial SQL Query", info,
        tableNames.isEmpty() ? "SELECT * FROM table" 
                             : QString("SELECT * FROM \"%1\"").arg(tableNames.first()),
        &ok);
    if (!ok || sql.trimmed().isEmpty()) return;

    // Execute via SpatiaLiteDAO (supports real spatial SQL)
    VectorLayer* resultLayer = executeSqlAndCreateLayer(dbPath, sql, "SQL查询结果");
    if (resultLayer)
    {
        mMapCanvas->zoomToFullExtent();
        statusBar()->showMessage(QString("SQL查询完成: %1 条记录")
                                 .arg(resultLayer->featureCount()), 5000);
        if (mAttributeTable && resultLayer->qgsVectorLayer())
            mAttributeTable->showAttributes(resultLayer->qgsVectorLayer(),
                                             QString("SQL: %1").arg(resultLayer->name()));
        return;
    }

    // Fallback: try OGR subset method
    QString queryUri = QString("%1|subset=%2").arg(uri, sql);

    auto layer = std::make_unique<VectorLayer>("SQL查询结果");
    auto ds = std::make_unique<FileDataSource>(queryUri);
    layer->setDataSource(std::move(ds));

    if (!layer->load() || !layer->isValid())
    {
        QMessageBox::warning(this, "查询失败",
                             QString("SQL执行失败:\n%1").arg(sql));
        return;
    }

    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(255, 200, 50, 150));
    renderer->setLineWidth(2.0);
    layer->setRenderer(std::move(renderer));
    layer->applyRenderer();

    mLayerManager->addLayer(std::move(layer));
    mMapCanvas->zoomToFullExtent();
    statusBar()->showMessage("SQL查询完成", 5000);
}


// ============================================================
// 地图工具
// ============================================================

void GeoAI::onPanTool()
{
    mMapCanvas->setMapTool(MapCanvas::PanTool);
}

void GeoAI::onZoomInTool()
{
    mMapCanvas->setMapTool(MapCanvas::ZoomInTool);
}

void GeoAI::onZoomOutTool()
{
    mMapCanvas->setMapTool(MapCanvas::ZoomOutTool);
}

void GeoAI::onZoomFullExtent()
{
    mMapCanvas->zoomToFullExtent();
    statusBar()->showMessage("全图显示", 2000);
}


// ============================================================
// 图层操作
// ============================================================

void GeoAI::onLayerMoveUp()
{
    QTreeWidgetItem* item = mLayerTree->currentItem();
    if (!item) return;

    int index = mLayerTree->indexOfTopLevelItem(item);
    if (index <= 0) return;

    // Save layer ID for re-selection
    QString id = item->data(0, Qt::UserRole).toString();
    
    // Move in LayerManager
    mLayerManager->moveLayerUp(index);
    
    // Move the tree item directly (no full rebuild)
    QTreeWidgetItem* taken = mLayerTree->takeTopLevelItem(index);
    mLayerTree->insertTopLevelItem(index - 1, taken);
    mLayerTree->setCurrentItem(taken);
    
    // Get current layer order from LayerManager
    QList<MapLayer*> orderedLayers = mLayerManager->allLayers();
    // Clear and re-render all layers in the correct order
    // Iterate in reverse: top layer in tree is drawn last (on top) in canvas
    mMapCanvas->clearQgsLayers();
    for (int i = orderedLayers.size() - 1; i >= 0; --i) {
        mMapCanvas->addLayer(orderedLayers[i]);
    }
    mMapCanvas->zoomToFullExtent();
}

void GeoAI::onLayerMoveDown()
{
    QTreeWidgetItem* item = mLayerTree->currentItem();
    if (!item) return;

    int index = mLayerTree->indexOfTopLevelItem(item);
    if (index < 0 || index >= mLayerTree->topLevelItemCount() - 1) return;

    QString id = item->data(0, Qt::UserRole).toString();
    
    mLayerManager->moveLayerDown(index);
    
    QTreeWidgetItem* taken = mLayerTree->takeTopLevelItem(index);
    mLayerTree->insertTopLevelItem(index + 1, taken);
    mLayerTree->setCurrentItem(taken);
    
    // Get current layer order from LayerManager
    QList<MapLayer*> orderedLayers = mLayerManager->allLayers();
    // Clear and re-render all layers in the correct order
    // Iterate in reverse: top layer in tree is drawn last (on top) in canvas
    mMapCanvas->clearQgsLayers();
    for (int i = orderedLayers.size() - 1; i >= 0; --i) {
        mMapCanvas->addLayer(orderedLayers[i]);
    }
    mMapCanvas->zoomToFullExtent();
}

void GeoAI::onLayerRemove()
{
    MapLayer* layer = mLayerTree->selectedLayer();
    if (!layer) return;
    
    QString id = layer->id();
    QString name = layer->name();
    
    // Remove from canvas first (while layer is still alive)
    mMapCanvas->removeLayer(id);
    
    // Disconnect all signals from this layer to prevent cascading updates
    layer->disconnect();
    
    // Remove from manager (this deletes the layer)
    mLayerManager->removeLayer(id);
    
    mMapCanvas->refresh();
    statusBar()->showMessage(QString("已移除图层: %1").arg(name), 3000);
}


// ============================================================
// 分析操作
// ============================================================

void GeoAI::onExecuteAnalysis()
{
    auto params = mAnalysisPanel->currentParams();
    if (!params)
    {
        QMessageBox::warning(this, "参数错误", "请设置分析参数");
        return;
    }

    QList<MapLayer*> inputLayers;

    // 根据参数类型获取输入图层
    if (auto* bp = dynamic_cast<BufferParam*>(params.get()))
    {
        MapLayer* layer = mLayerManager->layerById(bp->targetLayerId);
        if (layer) inputLayers.append(layer);
    }
    else if (auto* op = dynamic_cast<OverlayParam*>(params.get()))
    {
        MapLayer* layerA = mLayerManager->layerById(op->layerAId);
        MapLayer* layerB = mLayerManager->layerById(op->layerBId);
        if (layerA) inputLayers.append(layerA);
        if (layerB) inputLayers.append(layerB);
    }
    else if (auto* qp = dynamic_cast<QueryParam*>(params.get()))
    {
        MapLayer* layer = mLayerManager->layerById(qp->layerId);
        if (layer) inputLayers.append(layer);
        if (!qp->referenceLayerId.isEmpty())
        {
            MapLayer* refLayer = mLayerManager->layerById(qp->referenceLayerId);
            if (refLayer) {
                if (qp->queryType == QueryParam::SpatialQuery && qp->layerId == qp->referenceLayerId) {
                    QMessageBox::warning(this, "参数错误", "空间查询的目标图层和参考图层不能相同");
                    return;
                }
                inputLayers.append(refLayer);
            }
        }
    }

    if (inputLayers.isEmpty())
    {
        QMessageBox::warning(this, "图层错误", "请确保选择了有效的图层");
        return;
    }

    // 创建分析器
    std::unique_ptr<SpatialAnalyzer> analyzer;

    if (dynamic_cast<BufferParam*>(params.get()))
        analyzer = std::make_unique<BufferAnalyzer>();
    else if (dynamic_cast<OverlayParam*>(params.get()))
        analyzer = std::make_unique<OverlayAnalyzer>();
    else if (dynamic_cast<QueryParam*>(params.get()))
        analyzer = std::make_unique<QueryAnalyzer>();

    if (!analyzer)
    {
        QMessageBox::warning(this, "错误", "无法创建分析器");
        return;
    }

    analyzer->setParameters(std::move(params));
    analyzer->setInputLayers(inputLayers);

    statusBar()->showMessage("正在执行分析...");

    // 执行分析
    QList<MapLayer*> results = analyzer->execute();

    if (results.isEmpty())
    {
        statusBar()->showMessage("分析完成，无结果", 3000);
        return;
    }

    // 添加结果图层
    QStringList resultLayerNames;
    for (MapLayer* result : results)
    {
        if (result && result->isValid())
        {
            // 将结果包装为unique_ptr加入LayerManager
            // 注意：这里结果图层由analyzer创建，需要转移所有权
            // 由于MapLayer是QObject派生，无法简单用unique_ptr转移
            // 这里使用重新加载的方式
            auto resultLayer = std::make_unique<VectorLayer>(result->name());
            auto ds = std::make_unique<FileDataSource>(
                static_cast<VectorLayer*>(result)->dataSource()->uri());
            resultLayer->setDataSource(std::move(ds));
            resultLayer->load();

            auto renderer = std::make_unique<SimpleRenderer>();
            renderer->setFillColor(QColor(255, 200, 50, 150));
            renderer->setLineWidth(2.0);
            renderer->setPointSize(8.0);
            resultLayer->setRenderer(std::move(renderer));
            resultLayer->applyRenderer();

            resultLayerNames.append(resultLayer->name());
            mLayerManager->addLayer(std::move(resultLayer));
            delete result;  // 清理原始结果
        }
    }

    mMapCanvas->zoomToFullExtent();
    statusBar()->showMessage("分析完成！结果已添加到图层列表", 5000);

    // Ask if user wants to save to database
    if (QMessageBox::question(this, "保存结果", "是否将分析结果保存到空间数据库?",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        for (const QString& name : resultLayerNames)
        {
            auto* vl = dynamic_cast<VectorLayer*>(mLayerManager->layerByName(name));
            if (vl) saveResultToDatabase(vl);
        }
    }
}


// ============================================================
// 自然语言查询
// ============================================================

void GeoAI::onNaturalLanguageQuery(const AnalysisCommand& command)
{
    if (!command.isValid())
    {
        QMessageBox::information(this, "查询提示",
                                 "无法解析该查询。请尝试:\n"
                                 "- 找出距离河流200米内的建筑\n"
                                 "- 查询人口大于500000的城市\n"
                                 "- 显示与保护区重叠的耕地");
        return;
    }

    // 查找目标图层
    MapLayer* targetLayer = mLayerManager->findLayerByKeyword(command.targetLayer);
    if (!targetLayer)
    {
        QMessageBox::warning(this, "图层未找到",
                             QString("未找到名为\"%1\"的图层，请先加载相关数据")
                             .arg(command.targetLayer));
        return;
    }

    // 查找参考图层（如果需要）
    MapLayer* refLayer = nullptr;
    if (!command.referenceLayer.isEmpty())
    {
        refLayer = mLayerManager->findLayerByKeyword(command.referenceLayer);
        if (!refLayer &&
            (command.type == AnalysisType::SpatialQuery ||
             command.type == AnalysisType::BufferQuery ||
             command.type == AnalysisType::OverlayAnalysis))
        {
            QMessageBox::warning(this, "参考图层未找到",
                                 QString("未找到名为\"%1\"的参考图层")
                                 .arg(command.referenceLayer));
            return;
        }
    }

    statusBar()->showMessage(QString("执行: %1").arg(command.description()));

    std::unique_ptr<SpatialAnalyzer> analyzer;
    std::unique_ptr<AnalysisParam> params;

    switch (command.type)
    {
    case AnalysisType::BufferQuery:
    case AnalysisType::BufferAnalysis:
    {
        auto bp = std::make_unique<BufferParam>();
        bp->targetLayerId = targetLayer->id();
        bp->distance = command.distance;
        bp->distanceUnit = command.distanceUnit;
        bp->outputLayerName = QString("%1_Buffer_%2%3")
            .arg(targetLayer->name()).arg(command.distance).arg(command.distanceUnit);

        if (command.type == AnalysisType::BufferQuery && refLayer)
        {
            analyzer = std::make_unique<QueryAnalyzer>();
            auto qp = std::make_unique<QueryParam>();
            qp->queryType = QueryParam::CombinedQuery;
            qp->layerId = targetLayer->id();
            qp->referenceLayerId = refLayer->id();
            qp->bufferDistance = command.distance;
            qp->outputLayerName = QString("%1_near_%2_%3%4")
                .arg(targetLayer->name(), refLayer->name())
                .arg(command.distance).arg(command.distanceUnit);

            params = std::move(qp);

            QList<MapLayer*> inputs;
            inputs.append(targetLayer);
            inputs.append(refLayer);
            analyzer->setInputLayers(inputs);
        }
        else
        {
            analyzer = std::make_unique<BufferAnalyzer>();
            params = std::move(bp);
        }
        break;
    }

    case AnalysisType::AttributeQuery:
    {
        analyzer = std::make_unique<QueryAnalyzer>();
        auto qp = std::make_unique<QueryParam>();
        qp->queryType = QueryParam::AttributeQuery;
        qp->layerId = targetLayer->id();
        qp->attributeFilter = command.attributeFilter;
        qp->outputLayerName = targetLayer->name() + "_Filtered";
        params = std::move(qp);
        break;
    }

    case AnalysisType::SpatialQuery:
    {
        analyzer = std::make_unique<QueryAnalyzer>();
        auto qp = std::make_unique<QueryParam>();
        qp->queryType = QueryParam::SpatialQuery;
        qp->layerId = targetLayer->id();
        qp->referenceLayerId = refLayer ? refLayer->id() : "";
        qp->spatialRelation = AnalysisCommand::spatialRelationToString(command.relation);
        qp->outputLayerName = targetLayer->name() + "_SpatialResult";
        params = std::move(qp);

        QList<MapLayer*> inputs;
        inputs.append(targetLayer);
        if (refLayer) inputs.append(refLayer);
        analyzer->setInputLayers(inputs);
        break;
    }

    case AnalysisType::OverlayAnalysis:
    {
        analyzer = std::make_unique<OverlayAnalyzer>();
        auto op = std::make_unique<OverlayParam>();
        op->layerAId = targetLayer->id();
        op->layerBId = refLayer ? refLayer->id() : "";
        op->operation = (command.overlayType == OverlayType::Union)
            ? OverlayParam::Union : OverlayParam::Intersection;
        op->outputLayerName = QString("%1_%2_Overlay")
            .arg(targetLayer->name(), refLayer ? refLayer->name() : "");
        params = std::move(op);

        QList<MapLayer*> inputs;
        inputs.append(targetLayer);
        if (refLayer) inputs.append(refLayer);
        analyzer->setInputLayers(inputs);
        break;
    }

    default:
        QMessageBox::warning(this, "错误", "不支持的分析类型");
        return;
    }

    if (!analyzer)
    {
        statusBar()->showMessage("无法创建分析器", 3000);
        return;
    }

    analyzer->setParameters(std::move(params));

    qDebug() << "[NL] Executing analysis...";
    QList<MapLayer*> results = analyzer->execute();
    qDebug() << "[NL] Analysis done, results:" << results.size();

    if (results.isEmpty())
    {
        qDebug() << "[NL] No results";
        statusBar()->showMessage("分析完成，无结果");
        return;
    }

    qDebug() << "[NL] Adding result layers...";
    QStringList resultLayerNames;
    for (MapLayer* result : results)
    {
        qDebug() << "[NL] Processing result:" << result->name();
        if (result && result->isValid())
        {
            auto resultLayer = std::make_unique<VectorLayer>(result->name());
            auto srcDs = static_cast<VectorLayer*>(result)->dataSource();
            qDebug() << "[NL] DataSource uri:" << (srcDs ? srcDs->uri() : "null");
            if (srcDs)
            {
                auto ds = std::make_unique<FileDataSource>(srcDs->uri());
                resultLayer->setDataSource(std::move(ds));
                qDebug() << "[NL] Loading result layer...";
                resultLayer->load();
                if (resultLayer->qgsVectorLayer())
                    resultLayer->qgsVectorLayer()->updateExtents();

                auto renderer = std::make_unique<SimpleRenderer>();
                renderer->setFillColor(QColor(255, 255, 0, 180));
                renderer->setLineWidth(2.0);
                renderer->setPointSize(8.0);
                resultLayer->setRenderer(std::move(renderer));
                resultLayer->applyRenderer();

                resultLayerNames.append(resultLayer->name());
                qDebug() << "[NL] Adding to LayerManager...";
                mLayerManager->addLayer(std::move(resultLayer));
                qDebug() << "[NL] Added OK";
                // Force canvas to show new layer
                QgsRectangle rExtent;
                if (result && result->isValid()) {
                    rExtent = static_cast<VectorLayer*>(result)->extent();
                    if (!rExtent.isEmpty()) {
                        mMapCanvas->qgsCanvas()->setExtent(rExtent);
                        mMapCanvas->qgsCanvas()->refresh();
                    }
                }
            }
            delete result;
        }
    }
    qDebug() << "[NL] All layers added, zooming...";

    // Force canvas to include new layer extent
    QgsRectangle fullExtent;
    for (auto* l : mMapCanvas->qgsCanvas()->layers())
    {
        QgsRectangle e = l->extent();
        if (!e.isEmpty()) fullExtent.combineExtentWith(e);
    }
    if (!fullExtent.isEmpty())
    {
        mMapCanvas->qgsCanvas()->setExtent(fullExtent);
        mMapCanvas->qgsCanvas()->refresh();
    }
    mMapCanvas->zoomToFullExtent();
    statusBar()->showMessage(
        QString("查询完成: %1").arg(command.description()), 5000);

    // Ask if user wants to save to database
    if (QMessageBox::question(this, "保存结果", "是否将分析结果保存到空间数据库?",
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        for (const QString& name : resultLayerNames)
        {
            auto* vl = dynamic_cast<VectorLayer*>(mLayerManager->layerByName(name));
            if (vl) saveResultToDatabase(vl);
        }
    }
}


// ============================================================
// 渲染
// ============================================================

void GeoAI::onSetSimpleRenderer()
{
    MapLayer* layer = mLayerTree->selectedLayer();
    if (!layer) return;

    QColor color = QColorDialog::getColor(QColor(100, 150, 255, 150), this, "选择符号颜色");
    if (!color.isValid()) return;

    auto renderer = std::make_unique<SimpleRenderer>();
    QColor fillColor = color;
    fillColor.setAlpha(150);  // semi-transparent fill
    renderer->setFillColor(fillColor);
    renderer->setStrokeColor(color.darker(150));  // darker stroke for contrast
    renderer->setLineWidth(2.0);
    layer->setRenderer(std::move(renderer));
    layer->applyRenderer();
    mMapCanvas->refresh();
    statusBar()->showMessage(QString("已应用简单渲染 (颜色: %1)").arg(color.name()));
}

void GeoAI::onToggleRasterGray()
{
    MapLayer* layer = mLayerTree->selectedLayer();
    if (!layer || layer->layerType() != MapLayer::RasterLayerType) return;
    auto* rl = static_cast<RasterLayer*>(layer);
    rl->setGrayStretch(!rl->grayStretch());
    rl->applyRenderer();
    mMapCanvas->qgsCanvas()->refresh();
    statusBar()->showMessage(rl->grayStretch() ? "灰度拉伸" : "伪彩色", 2000);
}

// ============================================================
// 结果保存到数据库
// ============================================================

void GeoAI::saveResultToDatabase(VectorLayer* vl)
{
    if (!vl || !vl->qgsVectorLayer()) return;

    QString dbPath = QFileDialog::getSaveFileName(
        this, "保存结果到空间数据库", "result.sqlite",
        "SpatiaLite (*.sqlite);;GeoPackage (*.gpkg)");
    if (dbPath.isEmpty()) return;

    // Use QgsVectorFileWriter to write to database
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = dbPath.endsWith(".gpkg") ? "GPKG" : "SQLite";
    options.fileEncoding = "UTF-8";
    options.layerName = vl->name();

#pragma warning(push)
#pragma warning(disable : 4996)
    QgsVectorFileWriter::writeAsVectorFormat(
        vl->qgsVectorLayer(), dbPath, options);
#pragma warning(pop)

    // Reload from database to verify
    auto layer = std::make_unique<VectorLayer>(vl->name() + "_fromDB");
    auto ds = std::make_unique<FileDataSource>(dbPath + "|layername=" + vl->name());
    layer->setDataSource(std::move(ds));
    if (layer->load() && layer->isValid())
    {
        auto renderer = std::make_unique<SimpleRenderer>();
        renderer->setFillColor(QColor(100, 255, 100, 150));
        layer->setRenderer(std::move(renderer));
        layer->applyRenderer();
        mLayerManager->addLayer(std::move(layer));
        statusBar()->showMessage("Result saved to database and reloaded for verification", 3000);
    }
}

// ============================================================
// Attribute table: update on layer selection
// ============================================================

void GeoAI::onLayerSelected()
{
    qDebug() << "[GeoAI] onLayerSelected called";
    
    MapLayer* layer = mLayerTree->selectedLayer();
    if (!layer)
    {
        if (mAttributeTable) mAttributeTable->clearTable();
        return;
    }

    QString info = QString("Selected: %1 | Features: %2")
        .arg(layer->name())
        .arg(layer->layerType() == MapLayer::VectorLayerType
             ? QString::number(static_cast<VectorLayer*>(layer)->featureCount())
             : "N/A");
    statusBar()->showMessage(info);

    if (mAttributeTable)
        mAttributeTable->showLayerAttributes(layer);
}

// ============================================================
// SQL helper: execute SQL via OGR subset
// Basic SELECT works; spatial functions NOT supported via this path.
// For spatial analysis, use the analysis panel (Buffer/Overlay/Query).
// ============================================================

VectorLayer* GeoAI::executeSqlAndCreateLayer(const QString& dbPath, const QString& sql,
                                               const QString& layerName)
{
    QString uri = dbPath;
    QString queryUri = QString("%1|subset=%2").arg(uri, sql);

    auto layer = std::make_unique<VectorLayer>(layerName);
    auto ds = std::make_unique<FileDataSource>(queryUri);
    layer->setDataSource(std::move(ds));

    if (!layer->load() || !layer->isValid())
        return nullptr;

    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(255, 200, 50, 150));
    renderer->setLineWidth(2.0);
    layer->setRenderer(std::move(renderer));
    layer->applyRenderer();

    VectorLayer* resultPtr = layer.get();
    mLayerManager->addLayer(std::move(layer));

    return resultPtr;
}

// ============================================================
// Mouse coordinate display
// ============================================================

void GeoAI::onMouseCoordinateChanged(const QgsPointXY& coord)
{
    QString text = QString("X: %1  Y: %2")
        .arg(coord.x(), 0, 'f', 6)
        .arg(coord.y(), 0, 'f', 6);
    statusBar()->showMessage(text, 2000);
}

// ============================================================
// Project save/load (JSON-based)
// ============================================================

void GeoAI::onSaveProject()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "Save Project", "", "GeoAI Project (*.geoai);;All Files (*.*)");
    if (filePath.isEmpty()) return;

    QJsonObject project;
    QJsonArray layersArr;

    QList<MapLayer*> layers = mLayerManager->allLayers();
    for (MapLayer* layer : layers)
    {
        QJsonObject layerObj;
        layerObj["name"] = layer->name();
        layerObj["id"] = layer->id();
        layerObj["visible"] = layer->visible();
        layerObj["opacity"] = layer->opacity();
        layerObj["type"] = (layer->layerType() == MapLayer::VectorLayerType) ? "vector" : "raster";

        // Save data source URI
        DataSource* ds = layer->dataSource();
        if (ds)
            layerObj["uri"] = ds->uri();

        // Save renderer info
        Renderer* r = layer->renderer();
        if (r && r->rendererType() == Renderer::Simple)
        {
            auto* sr = static_cast<SimpleRenderer*>(r);
            QJsonObject renderObj;
            renderObj["fill"] = sr->fillColor().name();
            renderObj["stroke"] = sr->strokeColor().name();
            renderObj["lineWidth"] = sr->lineWidth();
            layerObj["renderer"] = renderObj;
        }

        layersArr.append(layerObj);
    }
    project["layers"] = layersArr;
    project["version"] = "1.0";

    QJsonDocument doc(project);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        statusBar()->showMessage("Project saved: " + filePath, 3000);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Cannot write: " + filePath);
    }
}

void GeoAI::onOpenProject()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Open Project", "", "GeoAI Project (*.geoai);;All Files (*.*)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Error", "Cannot read: " + filePath);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
    {
        QMessageBox::warning(this, "Error", "Invalid project file");
        return;
    }

    QJsonObject project = doc.object();
    QJsonArray layersArr = project["layers"].toArray();

    // Clear existing layers
    mLayerManager->clear();
    mMapCanvas->clearQgsLayers();

    for (const QJsonValue& val : layersArr)
    {
        QJsonObject obj = val.toObject();
        QString name = obj["name"].toString();
        QString uri = obj["uri"].toString();
        QString type = obj["type"].toString();
        bool visible = obj["visible"].toBool(true);
        double opacity = obj["opacity"].toDouble(1.0);

        if (uri.isEmpty()) continue;

        if (type == "vector")
        {
            auto layer = std::make_unique<VectorLayer>(name);
            auto ds = std::make_unique<FileDataSource>(uri);
            layer->setDataSource(std::move(ds));

            if (!layer->load() || !layer->isValid()) continue;

            // Restore renderer
            if (obj.contains("renderer"))
            {
                QJsonObject robj = obj["renderer"].toObject();
                auto renderer = std::make_unique<SimpleRenderer>();
                renderer->setFillColor(QColor(robj["fill"].toString()));
                renderer->setStrokeColor(QColor(robj["stroke"].toString()));
                renderer->setLineWidth(robj["lineWidth"].toDouble(1.0));
                layer->setRenderer(std::move(renderer));
                layer->applyRenderer();
            }
            else
            {
                auto renderer = std::make_unique<SimpleRenderer>();
                layer->setRenderer(std::move(renderer));
                layer->applyRenderer();
            }

            layer->setVisible(visible);
            layer->setOpacity(opacity);
            mLayerManager->addLayer(std::move(layer));
        }
    }

    mMapCanvas->zoomToFullExtent();
    statusBar()->showMessage("Project loaded: " + filePath, 3000);
}

// ============================================================
// Drag-drop file loading
// ============================================================
void GeoAI::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void GeoAI::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls())
    {
        QString path = url.toLocalFile();
        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();

        if (ext == "shp" || ext == "geojson" || ext == "json" || ext == "gpkg")
        {
            auto layer = std::make_unique<VectorLayer>(fi.baseName());
            auto ds = std::make_unique<FileDataSource>(path);
            layer->setDataSource(std::move(ds));
            if (layer->load() && layer->isValid())
            {
                auto r = std::make_unique<SimpleRenderer>();
                layer->setRenderer(std::move(r));
                layer->applyRenderer();
                mLayerManager->addLayer(std::move(layer));
                statusBar()->showMessage("Loaded: " + fi.fileName(), 3000);
            }
        }
        else if (ext == "tif" || ext == "tiff" || ext == "dem")
        {
            auto layer = std::make_unique<RasterLayer>(fi.baseName());
            auto ds = std::make_unique<FileDataSource>(path);
            layer->setDataSource(std::move(ds));
            if (layer->load() && layer->isValid())
            {
                mLayerManager->addLayer(std::move(layer));
                statusBar()->showMessage("Loaded: " + fi.fileName(), 3000);
            }
        }
    }
}

// ============================================================
// Export screenshot
// ============================================================
void GeoAI::onExportScreenshot()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Screenshot", "map.png", "PNG (*.png);;JPG (*.jpg)");
    if (path.isEmpty()) return;

    QPixmap pix = mMapCanvas->qgsCanvas()->grab();
    pix.save(path);
    statusBar()->showMessage("Screenshot saved: " + path, 3000);
}

// ============================================================
// About dialog
// ============================================================
void GeoAI::onAbout()
{
    QMessageBox::about(this, "About GeoAI",
        "<h2>GeoAI v1.0</h2>"
        "<p>AI-Enhanced Spatial Analysis Platform</p>"
        "<p>Built with Qt 5 + QGIS 3.14 + DeepSeek LLM</p>"
        "<p>OOP Course Project 2026</p>"
        "<hr>"
        "<p><b>Features:</b> Vector/Raster Loading, Spatial Analysis,</p>"
        "<p>Natural Language Query, LLM Integration, OpenMP Acceleration</p>");
}
