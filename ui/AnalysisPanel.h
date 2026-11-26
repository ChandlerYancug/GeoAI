#pragma once

#include <QWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <memory>

class LayerManager;
class MapCanvas;
class SpatialAnalyzer;
class AnalysisParam;

// ============================================================
// AnalysisPanel — 空间分析参数面板
//   OOP: 单一职责 — 只负责参数输入UI
//   分析执行由主窗口协调
// ============================================================
class AnalysisPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisPanel(QWidget* parent = nullptr);
    ~AnalysisPanel() override;

    void setLayerManager(LayerManager* manager);

    // Refresh layer dropdowns
    void refreshLayerLists();

    // Get current params
    std::unique_ptr<AnalysisParam> currentParams() const;

signals:
    void executeRequested();
    void analysisTypeChanged(int type);

private slots:
    void onAnalysisTypeChanged(int index);
    void onExecuteClicked();

private:
    void setupUI();
    void updateParamUI();

    LayerManager* mLayerManager = nullptr;

    // UI组件
    QComboBox* mAnalysisTypeCombo;
    QComboBox* mLayerComboA;
    QComboBox* mLayerComboB;
    QDoubleSpinBox* mDistanceSpinBox;
    QComboBox* mDistanceUnitCombo;
    QComboBox* mRelationCombo;
    QLineEdit* mAttributeFilterEdit;
    QPushButton* mExecuteButton;

    // 参数组容器
    QWidget* mBufferParamsWidget;
    QWidget* mOverlayParamsWidget;
    QWidget* mQueryParamsWidget;
};
