#include "AnalysisPanel.h"
#include "../core/MapLayer.h"
#include "../core/LayerManager.h"
#include "../analysis/SpatialAnalyzer.h"

#include <QGroupBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QScrollArea>

AnalysisPanel::AnalysisPanel(QWidget* parent) : QWidget(parent)
{
    setupUI();
}

AnalysisPanel::~AnalysisPanel() = default;

void AnalysisPanel::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 标题
    auto* titleLabel = new QLabel("空间分析工具", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    mainLayout->addWidget(titleLabel);

    // 分析类型选择
    auto* typeLayout = new QFormLayout();
    mAnalysisTypeCombo = new QComboBox(this);
    mAnalysisTypeCombo->addItems({"缓冲区分析", "叠加分析", "属性查询", "空间查询", "矢量裁剪"});
    typeLayout->addRow("分析类型:", mAnalysisTypeCombo);

    // 图层选择A
    mLayerComboA = new QComboBox(this);
    typeLayout->addRow("目标图层:", mLayerComboA);

    // 图层选择B
    mLayerComboB = new QComboBox(this);
    mLayerComboB->setVisible(false);
    typeLayout->addRow("参考图层:", mLayerComboB);

    mainLayout->addLayout(typeLayout);

    // 缓冲区参数组
    mBufferParamsWidget = new QWidget(this);
    auto* bufferLayout = new QFormLayout(mBufferParamsWidget);
    mDistanceSpinBox = new QDoubleSpinBox(this);
    mDistanceSpinBox->setRange(1, 100000);
    mDistanceSpinBox->setValue(100);
    mDistanceSpinBox->setSuffix(" 米");
    mDistanceSpinBox->setDecimals(1);
    bufferLayout->addRow("缓冲距离:", mDistanceSpinBox);

    mDistanceUnitCombo = new QComboBox(this);
    mDistanceUnitCombo->addItems({"米", "千米"});
    bufferLayout->addRow("距离单位:", mDistanceUnitCombo);
    mainLayout->addWidget(mBufferParamsWidget);

    // 叠加参数组
    mOverlayParamsWidget = new QWidget(this);
    auto* overlayLayout = new QFormLayout(mOverlayParamsWidget);
    mOverlayParamsWidget->setVisible(false);
    mainLayout->addWidget(mOverlayParamsWidget);

    // 查询参数组
    mQueryParamsWidget = new QWidget(this);
    auto* queryLayout = new QFormLayout(mQueryParamsWidget);
    mAttributeFilterEdit = new QLineEdit(this);
    mAttributeFilterEdit->setPlaceholderText("例: 人口 > 500000");
    queryLayout->addRow("属性条件:", mAttributeFilterEdit);

    mRelationCombo = new QComboBox(this);
    mRelationCombo->addItems({"相交 (intersects)", "包含 (contains)", "位于内部 (within)", "相邻 (touches)", "重叠 (overlaps)"});
    queryLayout->addRow("空间关系:", mRelationCombo);
    mQueryParamsWidget->setVisible(false);
    mainLayout->addWidget(mQueryParamsWidget);

    // 执行按钮
    mExecuteButton = new QPushButton("执行分析", this);
    mExecuteButton->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; padding: 8px; "
        "font-weight: bold; border-radius: 4px; }"
        "QPushButton:hover { background-color: #45a049; }");
    mainLayout->addWidget(mExecuteButton);

    mainLayout->addStretch();

    // 连接信号
    connect(mAnalysisTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPanel::onAnalysisTypeChanged);
    connect(mExecuteButton, &QPushButton::clicked,
            this, &AnalysisPanel::onExecuteClicked);

    onAnalysisTypeChanged(0);
}

void AnalysisPanel::setLayerManager(LayerManager* manager)
{
    if (mLayerManager)
    {
        disconnect(mLayerManager, &LayerManager::layerAdded,
                   this, nullptr);
        disconnect(mLayerManager, &LayerManager::layerRemoved,
                   this, nullptr);
        disconnect(mLayerManager, &LayerManager::layersCleared,
                   this, nullptr);
    }

    mLayerManager = manager;

    if (mLayerManager)
    {
        connect(mLayerManager, &LayerManager::layerAdded,
                this, [this](MapLayer*) { refreshLayerLists(); });
        connect(mLayerManager, &LayerManager::layerRemoved,
                this, [this](const QString&) { refreshLayerLists(); });
        connect(mLayerManager, &LayerManager::layersCleared,
                this, [this]() { refreshLayerLists(); });

        refreshLayerLists();
    }
}

void AnalysisPanel::refreshLayerLists()
{
    if (!mLayerManager) return;

    // Save current selections
    QString selectedA = mLayerComboA->currentData().toString();
    QString selectedB = mLayerComboB->currentData().toString();

    mLayerComboA->clear();
    mLayerComboB->clear();

    QList<MapLayer*> layers = mLayerManager->allLayers();
    for (MapLayer* layer : layers)
    {
        mLayerComboA->addItem(layer->name(), layer->id());
        mLayerComboB->addItem(layer->name(), layer->id());
    }

    // Restore selections
    int idxA = mLayerComboA->findData(selectedA);
    if (idxA >= 0) mLayerComboA->setCurrentIndex(idxA);
    int idxB = mLayerComboB->findData(selectedB);
    if (idxB >= 0) mLayerComboB->setCurrentIndex(idxB);
}

void AnalysisPanel::onAnalysisTypeChanged(int index)
{
    // 隐藏所有参数组
    mBufferParamsWidget->setVisible(index == 0);
    mOverlayParamsWidget->setVisible(index == 1);
    mQueryParamsWidget->setVisible(index >= 2);

    // 参考图层在叠加和空间查询时需要
    mLayerComboB->setVisible(index == 1 || index == 3);

    // 刷新图层下拉列表
    refreshLayerLists();

    emit analysisTypeChanged(index);
}

void AnalysisPanel::onExecuteClicked()
{
    emit executeRequested();
}

std::unique_ptr<AnalysisParam> AnalysisPanel::currentParams() const
{
    int type = mAnalysisTypeCombo->currentIndex();

    switch (type)
    {
    case 0:  // 缓冲区分析
    {
        auto params = std::make_unique<BufferParam>();
        params->targetLayerId = mLayerComboA->currentData().toString();
        params->distance = mDistanceSpinBox->value();
        params->distanceUnit = mDistanceUnitCombo->currentText();
        params->outputLayerName = QString("Buffer_%1m").arg(params->distance);
        return params;
    }

    case 1:  // 叠加分析
    {
        auto params = std::make_unique<OverlayParam>();
        params->layerAId = mLayerComboA->currentData().toString();
        params->layerBId = mLayerComboB->currentData().toString();
        params->operation = OverlayParam::Intersection;
        return params;
    }

    case 2:  // 属性查询
    {
        auto params = std::make_unique<QueryParam>();
        params->queryType = QueryParam::AttributeQuery;
        params->layerId = mLayerComboA->currentData().toString();
        params->attributeFilter = mAttributeFilterEdit->text();
        return params;
    }

    case 3:  // 空间查询
    {
        auto params = std::make_unique<QueryParam>();
        params->queryType = QueryParam::SpatialQuery;
        params->layerId = mLayerComboA->currentData().toString();
        params->referenceLayerId = mLayerComboB->currentData().toString();

        QString relText = mRelationCombo->currentText();
        if (relText.contains("intersects")) params->spatialRelation = "intersects";
        else if (relText.contains("contains")) params->spatialRelation = "contains";
        else if (relText.contains("within")) params->spatialRelation = "within";
        else if (relText.contains("touches")) params->spatialRelation = "touches";
        else if (relText.contains("overlaps")) params->spatialRelation = "overlaps";

        return params;
    }

    default:
        return nullptr;
    }
}
