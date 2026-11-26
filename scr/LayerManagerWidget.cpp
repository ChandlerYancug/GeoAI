/* FileName: LayerManagerWidget.cpp
   Author: YourName
   Version: 1.0
   Date: 2026-05-26
   Description: 图层管理器实现
*/

#include "LayerManagerWidget.h"
#include <QHeaderView>

LayerManagerWidget::LayerManagerWidget(QgsMapCanvas* pCanvas, QWidget* pParent)
    : QWidget(pParent)
    , m_pCanvas(pCanvas)
{
    setupUI();
}

LayerManagerWidget::~LayerManagerWidget()
{
}

void LayerManagerWidget::setupUI()
{
    QVBoxLayout* pMainLayout = new QVBoxLayout(this);

    m_pTreeWidget = new QTreeWidget(this);
    m_pTreeWidget->setHeaderLabel("Layers");
    m_pTreeWidget->setIndentation(10);
    m_pTreeWidget->setItemsExpandable(false);
    m_pTreeWidget->setRootIsDecorated(false);

    // 手动连接信号
    connect(m_pTreeWidget, &QTreeWidget::itemChanged,
        this, &LayerManagerWidget::onItemChanged);

    pMainLayout->addWidget(m_pTreeWidget);

    QHBoxLayout* pBtnLayout = new QHBoxLayout();
    m_pUpBtn = new QPushButton("Move Up", this);
    m_pDownBtn = new QPushButton("Move Down", this);

    connect(m_pUpBtn, &QPushButton::clicked, this, &LayerManagerWidget::onMoveUp);
    connect(m_pDownBtn, &QPushButton::clicked, this, &LayerManagerWidget::onMoveDown);

    pBtnLayout->addWidget(m_pUpBtn);
    pBtnLayout->addWidget(m_pDownBtn);
    pMainLayout->addLayout(pBtnLayout);

    setLayout(pMainLayout);
}

void LayerManagerWidget::addLayer(QgsMapLayer* pLayer)
{
    if (!pLayer) return;

    for (int i = 0; i < m_pTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* pItem = m_pTreeWidget->topLevelItem(i);
        if (pItem->data(0, Qt::UserRole).value<void*>() == pLayer) {
            return;
        }
    }

    QTreeWidgetItem* pItem = new QTreeWidgetItem(m_pTreeWidget);
    pItem->setText(0, pLayer->name());
    pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable);
    pItem->setCheckState(0, Qt::Checked);
    pItem->setData(0, Qt::UserRole, QVariant::fromValue((void*)pLayer));

    refreshLayers();
}

void LayerManagerWidget::removeLayer(QgsMapLayer* pLayer)
{
    for (int i = 0; i < m_pTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* pItem = m_pTreeWidget->topLevelItem(i);
        if (pItem->data(0, Qt::UserRole).value<void*>() == pLayer) {
            delete pItem;
            break;
        }
    }
    refreshLayers();
}

void LayerManagerWidget::refreshLayers()
{
    if (!m_pCanvas) return;

    QList<QgsMapLayer*> vLayers;
    for (int i = 0; i < m_pTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* pItem = m_pTreeWidget->topLevelItem(i);
        // 只有勾选的图层才加入列表
        if (pItem->checkState(0) == Qt::Checked) {
            QgsMapLayer* pLayer = (QgsMapLayer*)pItem->data(0, Qt::UserRole).value<void*>();
            if (pLayer) {
                vLayers.append(pLayer);
            }
        }
    }

    m_pCanvas->setLayers(vLayers);
    m_pCanvas->refresh();
}

void LayerManagerWidget::onItemChanged(QTreeWidgetItem* pItem, int nColumn)
{
    Q_UNUSED(nColumn);
    refreshLayers();
}

void LayerManagerWidget::onMoveUp()
{
    QTreeWidgetItem* pCurrent = m_pTreeWidget->currentItem();
    if (!pCurrent) return;

    int nIndex = m_pTreeWidget->indexOfTopLevelItem(pCurrent);
    if (nIndex <= 0) return;

    QTreeWidgetItem* pTakeItem = m_pTreeWidget->takeTopLevelItem(nIndex);
    m_pTreeWidget->insertTopLevelItem(nIndex - 1, pTakeItem);
    m_pTreeWidget->setCurrentItem(pTakeItem);
    updateLayerOrder();
}

void LayerManagerWidget::onMoveDown()
{
    QTreeWidgetItem* pCurrent = m_pTreeWidget->currentItem();
    if (!pCurrent) return;

    int nIndex = m_pTreeWidget->indexOfTopLevelItem(pCurrent);
    if (nIndex >= m_pTreeWidget->topLevelItemCount() - 1) return;

    QTreeWidgetItem* pTakeItem = m_pTreeWidget->takeTopLevelItem(nIndex);
    m_pTreeWidget->insertTopLevelItem(nIndex + 1, pTakeItem);
    m_pTreeWidget->setCurrentItem(pTakeItem);
    updateLayerOrder();
}

void LayerManagerWidget::updateLayerOrder()
{
    if (!m_pCanvas) return;

    QList<QgsMapLayer*> vLayers;
    for (int i = 0; i < m_pTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* pItem = m_pTreeWidget->topLevelItem(i);
        QgsMapLayer* pLayer = (QgsMapLayer*)pItem->data(0, Qt::UserRole).value<void*>();
        if (pLayer) {
            vLayers.append(pLayer);
        }
    }

    m_pCanvas->setLayers(vLayers);
    m_pCanvas->refresh();
}