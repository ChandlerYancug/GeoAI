#include "LayerTreeWidget.h"
#include "../core/MapLayer.h"
#include "../core/LayerManager.h"

#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <qgsrasterlayer.h>
#include <qgsrasterbandstats.h>

LayerTreeWidget::LayerTreeWidget(QWidget* parent) : QTreeWidget(parent)
{
    setupUI();
}

LayerTreeWidget::~LayerTreeWidget() = default;

void LayerTreeWidget::setupUI()
{
    setHeaderLabels({"图层名称", "类型", "要素数"});
    header()->setStretchLastSection(true);
    setColumnWidth(0, 180);
    setColumnWidth(1, 80);
    setColumnWidth(2, 80);

    // 拖拽调整顺序已禁用 — 与自定义移动代码冲突，会导致图层消失
    // setDragEnabled(true);
    // setAcceptDrops(true);
    // setDragDropMode(QAbstractItemView::InternalMove);

    // 复选框用于可见性
    setSelectionMode(QAbstractItemView::SingleSelection);

    // 右键上下文菜单
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTreeWidget::customContextMenuRequested,
            this, &LayerTreeWidget::onContextMenu);

    connect(this, &QTreeWidget::itemChanged,
            this, &LayerTreeWidget::onItemChanged);
    connect(this, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
                QString id = item->data(0, Qt::UserRole).toString();
                emit layerDoubleClicked(id);
            });
}

void LayerTreeWidget::setLayerManager(LayerManager* manager)
{
    if (mLayerManager)
    {
        disconnect(mLayerManager, &LayerManager::layerAdded,
                   this, &LayerTreeWidget::onLayerAdded);
        disconnect(mLayerManager, &LayerManager::layerRemoved,
                   this, &LayerTreeWidget::onLayerRemoved);
    }

    mLayerManager = manager;

    if (mLayerManager)
    {
        connect(mLayerManager, &LayerManager::layerAdded,
                this, &LayerTreeWidget::onLayerAdded);
        connect(mLayerManager, &LayerManager::layerRemoved,
                this, &LayerTreeWidget::onLayerRemoved);
        connect(mLayerManager, &LayerManager::layersCleared,
                this, [this]() { clear(); });

        refreshTree();
    }
}

LayerManager* LayerTreeWidget::layerManager() const
{
    return mLayerManager;
}

void LayerTreeWidget::refreshTree()
{
    clear();

    if (!mLayerManager) return;

    QList<MapLayer*> layers = mLayerManager->allLayers();
    for (MapLayer* layer : layers)
    {
        addLayerItem(layer);
    }
}

void LayerTreeWidget::addLayerItem(MapLayer* layer)
{
    if (!layer) return;

    auto* item = new QTreeWidgetItem(this);
    item->setText(0, layer->name());
    item->setData(0, Qt::UserRole, layer->id());

    // 类型列
    if (layer->layerType() == MapLayer::VectorLayerType)
    {
        auto* vl = static_cast<VectorLayer*>(layer);
        item->setText(1, vl->geometryType() == VectorLayer::Point ? "点" :
                         vl->geometryType() == VectorLayer::Line ? "线" : "面");
        item->setText(2, QString::number(vl->featureCount()));
    }
    else
    {
        item->setText(1, "栅格");
        item->setText(2, "");
    }

    // 可见性复选框
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, layer->visible() ? Qt::Checked : Qt::Unchecked);
}

QTreeWidgetItem* LayerTreeWidget::findItemByLayerId(const QString& id) const
{
    for (int i = 0; i < topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == id)
            return item;
    }
    return nullptr;
}

MapLayer* LayerTreeWidget::selectedLayer() const
{
    QTreeWidgetItem* item = currentItem();
    if (!item || !mLayerManager) return nullptr;

    QString id = item->data(0, Qt::UserRole).toString();
    return mLayerManager->layerById(id);
}

void LayerTreeWidget::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = itemAt(pos);
    if (!item) return;

    setCurrentItem(item);

    QMenu menu(this);
    QAction* deleteAction = menu.addAction("删除图层");
    QAction* moveUpAction = menu.addAction("上移");
    QAction* moveDownAction = menu.addAction("下移");
    QAction* opacityAction = menu.addAction("设置透明度...");
    menu.addSeparator();
    QAction* fieldsAction = menu.addAction("查看字段");
    QAction* labelsAction = menu.addAction("显示标签...");
    menu.addSeparator();
    QAction* bandAction = menu.addAction("查看波段");
    menu.addSeparator();
    QAction* classifiedAction = menu.addAction("分类渲染...");
    QAction* graduatedAction = menu.addAction("分级渲染...");
    menu.addSeparator();
    QAction* sldAction = menu.addAction("导出SLD...");

    QAction* selected = menu.exec(viewport()->mapToGlobal(pos));
    if (!selected) return;

    QString id = item->data(0, Qt::UserRole).toString();

    if (selected == deleteAction)
        emit layerRemovedRequested(id);
    else if (selected == moveUpAction)
        emit layerMoveUpRequested(id);
    else if (selected == moveDownAction)
        emit layerMoveDownRequested(id);
    else if (selected == opacityAction)
        emit layerOpacityRequested(id);
    else if (selected == fieldsAction)
        emit layerFieldsRequested(id);
    else if (selected == labelsAction)
        emit layerLabelsRequested(id);
    else if (selected == bandAction)
    {
        MapLayer* layer = mLayerManager->layerById(id);
        auto* rl = dynamic_cast<RasterLayer*>(layer);
        if (rl && rl->qgsRasterLayer())
        {
            int bands = rl->bandCount();
            QString info = QString("Raster: %1\nBands: %2\n\n").arg(layer->name()).arg(bands);
            for (int b = 1; b <= bands; ++b)
            {
                auto stats = rl->qgsRasterLayer()->dataProvider()->bandStatistics(b,
                    QgsRasterBandStats::Min | QgsRasterBandStats::Max | QgsRasterBandStats::Mean, QgsRectangle(), 250000);
                info += QString("Band %1: min=%2 max=%3 mean=%4\n")
                    .arg(b).arg(stats.minimumValue, 0, 'f', 2)
                    .arg(stats.maximumValue, 0, 'f', 2)
                    .arg(stats.mean, 0, 'f', 2);
            }
            QMessageBox::information(this, "Band Info", info);
        }
    }
    else if (selected == classifiedAction)
        emit layerClassifiedRequested(id);
    else if (selected == graduatedAction)
        emit layerGraduatedRequested(id);
    else if (selected == sldAction)
        emit layerSldExportRequested(id);
}

void LayerTreeWidget::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 0 || !mLayerManager) return;

    QString id = item->data(0, Qt::UserRole).toString();
    bool visible = (item->checkState(0) == Qt::Checked);

    mLayerManager->setLayerVisible(id, visible);
    emit layerVisibilityToggled(id, visible);
}

void LayerTreeWidget::onLayerAdded(MapLayer* layer)
{
    addLayerItem(layer);
}

void LayerTreeWidget::onLayerRemoved(const QString& id)
{
    QTreeWidgetItem* item = findItemByLayerId(id);
    if (item)
        delete item;
}
