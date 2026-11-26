#include "AttributeTableWidget.h"
#include "../core/MapLayer.h"
#include "../core/LayerManager.h"

#include <QHeaderView>
#include <QScrollBar>
#include <qgsvectorlayer.h>
#include <qgsfeatureiterator.h>
#include <qgsfeature.h>
#include <QDebug>

AttributeTableWidget::AttributeTableWidget(QWidget* parent) : QWidget(parent)
{
    setupUI();
}

AttributeTableWidget::~AttributeTableWidget() = default;

void AttributeTableWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    mTitleLabel = new QLabel("Attribute Table", this);
    mTitleLabel->setStyleSheet("font-weight: bold; font-size: 13px; padding: 2px;");
    layout->addWidget(mTitleLabel);

    mTable = new QTableWidget(this);
    mTable->setAlternatingRowColors(true);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->horizontalHeader()->setStretchLastSection(true);
    mTable->verticalHeader()->setVisible(true);
    mTable->setSortingEnabled(false);
    layout->addWidget(mTable, 1);

    mStatusLabel = new QLabel("No layer selected", this);
    mStatusLabel->setStyleSheet("color: gray; font-size: 11px;");
    layout->addWidget(mStatusLabel);
}

void AttributeTableWidget::setLayerManager(LayerManager* manager)
{
    mLayerManager = manager;
}

void AttributeTableWidget::showLayerAttributes(MapLayer* layer)
{
    if (!layer)
    {
        qDebug() << "[AttrTable] layer is null";
        clearTable();
        return;
    }

    qDebug() << "[AttrTable] showLayerAttributes:" << layer->name()
             << "layerType=" << layer->layerType();

    if (layer->layerType() != MapLayer::VectorLayerType)
    {
        mStatusLabel->setText(QString("Not vector: %1").arg(layer->name()));
        clearTable();
        return;
    }

    auto* vl = static_cast<VectorLayer*>(layer);
    QgsVectorLayer* qvl = vl->qgsVectorLayer();
    if (!qvl || !qvl->isValid())
    {
        mStatusLabel->setText(QString("Invalid: %1").arg(layer->name()));
        clearTable();
        return;
    }

    qDebug() << "[AttrTable] OK, featureCount=" << qvl->featureCount()
             << "fields=" << qvl->fields().size();

    mTitleLabel->setText(QString("Attribute: %1").arg(layer->name()));
    populateTable(qvl);
}

void AttributeTableWidget::showAttributes(QgsVectorLayer* vlayer, const QString& title)
{
    if (!vlayer || !vlayer->isValid())
    {
        clearTable();
        return;
    }

    mTitleLabel->setText(title.isEmpty() ? "Attribute Table" : title);
    populateTable(vlayer);
}

void AttributeTableWidget::clearTable()
{
    mTable->clear();
    mTable->setRowCount(0);
    mTable->setColumnCount(0);
    mTitleLabel->setText("Attribute Table");
    mStatusLabel->setText("No layer selected");
}

void AttributeTableWidget::populateTable(QgsVectorLayer* vlayer, int maxRows)
{
    if (!vlayer) return;

    const QgsFields& fields = vlayer->fields();
    int fieldCount = fields.size();

    qDebug() << "[AttrTable] populateTable: fieldCount=" << fieldCount;

    // Build header list, filtering out geometry columns
    QStringList headers;
    headers << "FID";
    QList<int> fieldIndexMap;

    for (int i = 0; i < fieldCount; ++i)
    {
        QString name = fields.at(i).name();
        if (name.toLower() != "geom" && name.toLower() != "geometry")
        {
            headers << name;
            fieldIndexMap.append(i);
        }
    }

    int displayCols = headers.size();
    mTable->setColumnCount(displayCols);
    mTable->setHorizontalHeaderLabels(headers);

    // Read features
    QgsFeatureIterator it = vlayer->getFeatures();
    QgsFeature feat;
    int row = 0;

    int totalFeatures = static_cast<int>(vlayer->featureCount());
    int rowsToRead = qMin(totalFeatures, maxRows);

    qDebug() << "[AttrTable] totalFeatures=" << totalFeatures
             << "rowsToRead=" << rowsToRead;

    mTable->setRowCount(rowsToRead);

    while (it.nextFeature(feat) && row < rowsToRead)
    {
        // FID column
        auto* fidItem = new QTableWidgetItem(QString::number(feat.id()));
        fidItem->setData(Qt::UserRole, static_cast<qint64>(feat.id()));
        mTable->setItem(row, 0, fidItem);

        // Attribute columns
        for (int col = 0; col < fieldIndexMap.size(); ++col)
        {
            int srcIdx = fieldIndexMap[col];
            QVariant val = feat.attribute(srcIdx);
            QString text;
            if (val.isNull())
                text = "NULL";
            else if (val.type() == QVariant::Double)
                text = QString::number(val.toDouble(), 'f', 4);
            else
                text = val.toString();

            mTable->setItem(row, col + 1, new QTableWidgetItem(text));
        }

        ++row;
    }

    mTable->resizeColumnsToContents();

    QString statusText;
    if (totalFeatures > maxRows)
        statusText = QString("Showing %1 / %2 (limit %3)")
            .arg(rowsToRead).arg(totalFeatures).arg(maxRows);
    else
        statusText = QString("Total: %1 records").arg(totalFeatures);

    mStatusLabel->setText(statusText);
    qDebug() << "[AttrTable] done:" << statusText;
}
