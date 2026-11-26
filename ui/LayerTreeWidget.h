#pragma once

#include <QTreeWidget>
#include <QObject>
#include <QMenu>

class LayerManager;
class MapLayer;

// ============================================================
// LayerTreeWidget — 图层树控件
//   OOP: 观察者模式 — 监听LayerManager变化并更新UI
//   支持图层可见性开关、顺序调整、右键菜单
// ============================================================
class LayerTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit LayerTreeWidget(QWidget* parent = nullptr);
    ~LayerTreeWidget() override;

    // 绑定LayerManager
    void setLayerManager(LayerManager* manager);
    LayerManager* layerManager() const;

    // 刷新显示
    void refreshTree();

    // 获取选中的图层
    MapLayer* selectedLayer() const;

signals:
    void layerSelected(MapLayer* layer);
    void layerVisibilityToggled(const QString& layerId, bool visible);
    void layerMoveUpRequested(const QString& layerId);
    void layerMoveDownRequested(const QString& layerId);
    void layerRemovedRequested(const QString& layerId);
    void layerOpacityRequested(const QString& layerId);
    void layerFieldsRequested(const QString& layerId);
    void layerLabelsRequested(const QString& layerId);
    void layerDoubleClicked(const QString& layerId);
    void layerClassifiedRequested(const QString& layerId);
    void layerGraduatedRequested(const QString& layerId);
    void layerSldExportRequested(const QString& layerId);

private slots:
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onLayerAdded(MapLayer* layer);
    void onLayerRemoved(const QString& id);
    void onContextMenu(const QPoint& pos);

private:
    LayerManager* mLayerManager = nullptr;

    void setupUI();
    void addLayerItem(MapLayer* layer);
    QTreeWidgetItem* findItemByLayerId(const QString& id) const;
};
