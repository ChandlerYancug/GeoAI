#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QLabel>

class MapLayer;
class VectorLayer;
class LayerManager;

// ============================================================
// AttributeTableWidget — 要素属性表控件
//   当用户在图层树选中图层时，显示该图层的要素属性表
//   OOP: 单一职责 — 只负责属性表UI展示
// ============================================================
class AttributeTableWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AttributeTableWidget(QWidget* parent = nullptr);
    ~AttributeTableWidget() override;

    // 绑定图层管理器
    void setLayerManager(LayerManager* manager);

    // 显示指定图层的属性表
    void showLayerAttributes(MapLayer* layer);

    // 直接展示QgsVectorLayer的属性（用于SQL查询结果）
    void showAttributes(class QgsVectorLayer* vlayer, const QString& title);

    // 清空表格
    void clearTable();

signals:
    void featureSelected(qint64 featureId);

private:
    void setupUI();
    void populateTable(QgsVectorLayer* vlayer, int maxRows = 1000);

    LayerManager* mLayerManager = nullptr;
    QTableWidget* mTable;
    QLabel* mTitleLabel;
    QLabel* mStatusLabel;
};
