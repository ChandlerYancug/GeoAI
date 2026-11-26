#pragma warning(push)
#pragma warning(disable: 4996)

#include <omp.h>

#include "Analyzers.h"
#include "../core/AnalysisCommand.h"

#include <qgsvectorlayer.h>
#include <qgsfeaturerequest.h>
#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsgeometryengine.h>
#include <qgsvectorfilewriter.h>
#include <qgscoordinatereferencesystem.h>
#include <qgscoordinatetransform.h>
#include <qgsproject.h>

#include <QUuid>
#include <QDir>
#include <QFileInfo>

#include "../core/DataSource.h"
#include "../core/Renderer.h"

// ============================================================
// BufferAnalyzer 实现
// ============================================================

BufferAnalyzer::BufferAnalyzer(QObject* parent) : SpatialAnalyzer(parent) {}
BufferAnalyzer::~BufferAnalyzer() = default;

QString BufferAnalyzer::description() const
{
    auto* bp = dynamic_cast<BufferParam*>(mParams.get());
    if (bp)
        return QString("对图层生成 %1 米的缓冲区").arg(bp->distance);
    return "缓冲区分析";
}

QList<MapLayer*> BufferAnalyzer::execute()
{
    QList<MapLayer*> results;
    auto* bp = dynamic_cast<BufferParam*>(mParams.get());
    if (!bp || mInputLayers.isEmpty()) return results;

    VectorLayer* source = dynamic_cast<VectorLayer*>(mInputLayers.first());
    if (!source || !source->isValid()) return results;

    // 单位转换 (米为基础单位)
    double distance = bp->distance;
    QString unit = bp->distanceUnit.toLower();
    if (unit == "km" || unit == "千米" || unit == "公里")
        distance *= 1000.0;

    VectorLayer* result = createBufferLayer(source, distance, bp->segments);
    if (result)
    {
        result->setName(bp->outputLayerName.isEmpty()
                        ? QString("%1_Buffer_%2m").arg(source->name()).arg(distance)
                        : bp->outputLayerName);
        results.append(result);
    }

    return results;
}

VectorLayer* BufferAnalyzer::createBufferLayer(VectorLayer* source,
                                                double distance, int segments)
{
    if (!source || !source->qgsVectorLayer()) return nullptr;

    QgsVectorLayer* srcLayer = source->qgsVectorLayer();

    // 创建内存图层存储缓冲结果
    QString uri = QString("Polygon?crs=%1").arg(srcLayer->crs().authid());
    auto memLayer = new QgsVectorLayer(uri, "buffer_result", "memory");
    if (!memLayer->isValid())
    {
        delete memLayer;
        return nullptr;
    }

    // 复制字段结构
    memLayer->dataProvider()->addAttributes(srcLayer->fields().toList());
    memLayer->updateFields();

    memLayer->startEditing();

    // Collect all features for parallel processing
    QgsFeatureList featureList;
    QgsFeatureIterator it = srcLayer->getFeatures();
    QgsFeature feat;
    while (it.nextFeature(feat))
        featureList.append(feat);

    int nFeatures = static_cast<int>(featureList.size());
    QVector<QgsFeature> results(nFeatures);

#pragma omp parallel for
    for (int i = 0; i < nFeatures; ++i)
    {
        const QgsFeature& f = featureList[i];
        if (!f.hasGeometry()) continue;

        QgsGeometry buffered = f.geometry().buffer(distance, segments);
        if (buffered.isNull()) continue;

        QgsFeature newFeat(memLayer->fields());
        newFeat.setGeometry(buffered);
        const QgsAttributes& attrs = f.attributes();
        for (int a = 0; a < attrs.size(); ++a)
            newFeat.setAttribute(a, attrs[a]);
        results[i] = newFeat;
    }

    // Add results sequentially (memory layer not thread-safe)
    for (int i = 0; i < nFeatures; ++i)
    {
        if (results[i].isValid() && results[i].hasGeometry())
            memLayer->addFeature(results[i]);
    }

    memLayer->commitChanges();

    // 包装为VectorLayer
    auto layer = std::make_unique<VectorLayer>(memLayer->name());
    // 直接设置QgsVectorLayer（绕过DataSource加载）
    Q_UNUSED(layer);
    // 使用一个特殊的方式：创建临时文件，然后用VectorLayer加载

    // 保存为临时GeoPackage文件
    QString tmpPath = QDir::tempPath() + "/geoai_buffer_" +
                      QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8) + ".gpkg";

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = "GPKG";
    options.fileEncoding = "UTF-8";
    QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, options);

    // 清理内存图层
    delete memLayer;

    // 创建VectorLayer加载临时文件
    auto resultLayer = new VectorLayer("buffer_result");
    auto ds = std::make_unique<FileDataSource>(tmpPath);
    resultLayer->setDataSource(std::move(ds));
    resultLayer->load();

    // 简单渲染
    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(255, 100, 100, 120));
    renderer->setStrokeColor(QColor(200, 0, 0));
    resultLayer->setRenderer(std::move(renderer));
    resultLayer->applyRenderer();

    return resultLayer;
}


// ============================================================
// OverlayAnalyzer 实现
// ============================================================

OverlayAnalyzer::OverlayAnalyzer(QObject* parent) : SpatialAnalyzer(parent) {}
OverlayAnalyzer::~OverlayAnalyzer() = default;

QString OverlayAnalyzer::description() const
{
    auto* op = dynamic_cast<OverlayParam*>(mParams.get());
    if (op)
    {
        QString opName = (op->operation == OverlayParam::Intersection) ? "交集" : "并集";
        return QString("两个图层的%1分析").arg(opName);
    }
    return "叠加分析";
}

QList<MapLayer*> OverlayAnalyzer::execute()
{
    QList<MapLayer*> results;
    auto* op = dynamic_cast<OverlayParam*>(mParams.get());
    if (!op || mInputLayers.size() < 2) return results;

    VectorLayer* layerA = dynamic_cast<VectorLayer*>(mInputLayers[0]);
    VectorLayer* layerB = dynamic_cast<VectorLayer*>(mInputLayers[1]);
    if (!layerA || !layerB) return results;

    VectorLayer* result = nullptr;
    QString outputName = op->outputLayerName.isEmpty()
        ? QString("%1_%2_Overlay").arg(layerA->name(), layerB->name())
        : op->outputLayerName;

    switch (op->operation)
    {
    case OverlayParam::Intersection:
        result = performIntersection(layerA, layerB, outputName);
        break;
    case OverlayParam::Union:
        result = performUnion(layerA, layerB, outputName);
        break;
    default:
        break;
    }

    if (result) results.append(result);
    return results;
}

VectorLayer* OverlayAnalyzer::performIntersection(VectorLayer* layerA, VectorLayer* layerB,
                                                    const QString& outputName)
{
    if (!layerA || !layerB) return nullptr;
    QgsVectorLayer* srcA = layerA->qgsVectorLayer();
    QgsVectorLayer* srcB = layerB->qgsVectorLayer();
    if (!srcA || !srcB) return nullptr;

    qDebug() << "[Overlay] A:" << srcA->name() << "features:" << srcA->featureCount()
             << "CRS:" << srcA->crs().authid();
    qDebug() << "[Overlay] B:" << srcB->name() << "features:" << srcB->featureCount()
             << "CRS:" << srcB->crs().authid();

    // Fix empty CRS
    if (!srcA->crs().isValid()) srcA->setCrs(QgsCoordinateReferenceSystem("EPSG:4326"));
    if (!srcB->crs().isValid()) srcB->setCrs(QgsCoordinateReferenceSystem("EPSG:4326"));

    QString geomStr = QgsWkbTypes::displayString(srcA->wkbType());
    if (geomStr.isEmpty()) geomStr = "Point";

    // 创建内存图层
    QString uri = QString("%1?crs=%2").arg(geomStr, srcA->crs().authid());
    auto memLayer = new QgsVectorLayer(uri, outputName, "memory");
    memLayer->dataProvider()->addAttributes(srcA->fields().toList());
    memLayer->updateFields();
    memLayer->startEditing();

    // Collect B features for efficient intersection testing
    QgsFeatureList featuresB;
    QgsFeatureIterator itB_all = srcB->getFeatures();
    QgsFeature fb;
    while (itB_all.nextFeature(fb))
        if (fb.hasGeometry()) featuresB.append(fb);

    QgsFeatureIterator itA = srcA->getFeatures();
    QgsFeature featA;
    int totalA = 0, matched = 0;
    while (itA.nextFeature(featA))
    {
        totalA++;
        if (!featA.hasGeometry()) continue;

        for (const QgsFeature& featB : featuresB)
        {
            QgsGeometry intersected = featA.geometry().intersection(featB.geometry());
            if (intersected.isNull() || intersected.isEmpty()) continue;

            QgsFeature newFeat(memLayer->fields());
            newFeat.setGeometry(intersected);
            const QgsAttributes& attrs = featA.attributes();
            for (int i = 0; i < attrs.size() && i < memLayer->fields().size(); ++i)
                newFeat.setAttribute(i, attrs[i]);
            memLayer->addFeature(newFeat);
            matched++;
        }
    }
    qDebug() << "[Overlay] Total A features:" << totalA << "B features:" << featuresB.size() << "matched:" << matched;

    memLayer->commitChanges();

    // 写入临时文件
    QString tmpPath = QDir::tempPath() + "/geoai_intersect_" +
                      QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8) + ".gpkg";

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = "GPKG";
    options.fileEncoding = "UTF-8";
    QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, options);

    delete memLayer;

    auto resultLayer = new VectorLayer(outputName);
    auto ds = std::make_unique<FileDataSource>(tmpPath);
    resultLayer->setDataSource(std::move(ds));
    resultLayer->load();

    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(100, 200, 100, 150));
    resultLayer->setRenderer(std::move(renderer));
    resultLayer->applyRenderer();

    return resultLayer;
}

VectorLayer* OverlayAnalyzer::performUnion(VectorLayer* layerA, VectorLayer* layerB,
                                            const QString& outputName)
{
    if (!layerA || !layerB) return nullptr;
    QgsVectorLayer* srcA = layerA->qgsVectorLayer();
    QgsVectorLayer* srcB = layerB->qgsVectorLayer();
    if (!srcA || !srcB) return nullptr;

    QString geomStr = QgsWkbTypes::displayString(srcA->wkbType());
    if (geomStr.isEmpty()) geomStr = "Point";

    QString uri = QString("%1?crs=%2").arg(geomStr, srcA->crs().authid());
    auto memLayer = new QgsVectorLayer(uri, outputName, "memory");
    memLayer->dataProvider()->addAttributes(srcA->fields().toList());
    memLayer->updateFields();
    memLayer->startEditing();

    // Add all A features
    QgsFeatureIterator itA = srcA->getFeatures();
    QgsFeature feat;
    while (itA.nextFeature(feat))
        memLayer->addFeature(feat);

    // Add all B features
    QgsFeatureIterator itB = srcB->getFeatures();
    while (itB.nextFeature(feat))
    {
        QgsFeature newFeat(memLayer->fields());
        newFeat.setGeometry(feat.geometry());
        memLayer->addFeature(newFeat);
    }

    memLayer->commitChanges();

    // 写入临时文件
    QString tmpPath = QDir::tempPath() + "/geoai_union_" +
                      QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8) + ".gpkg";

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = "GPKG";
    options.fileEncoding = "UTF-8";
    QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, options);

    delete memLayer;

    auto resultLayer = new VectorLayer(outputName);
    auto ds = std::make_unique<FileDataSource>(tmpPath);
    resultLayer->setDataSource(std::move(ds));
    resultLayer->load();

    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(100, 100, 255, 150));
    resultLayer->setRenderer(std::move(renderer));
    resultLayer->applyRenderer();

    return resultLayer;
}


// ============================================================
// QueryAnalyzer 实现
// ============================================================

QueryAnalyzer::QueryAnalyzer(QObject* parent) : SpatialAnalyzer(parent) {}
QueryAnalyzer::~QueryAnalyzer() = default;

QString QueryAnalyzer::description() const
{
    auto* qp = dynamic_cast<QueryParam*>(mParams.get());
    if (qp)
    {
        if (qp->queryType == QueryParam::AttributeQuery)
            return QString("属性查询: %1").arg(qp->attributeFilter);
        else
            return QString("空间查询: %1").arg(qp->spatialRelation);
    }
    return "查询分析";
}

QList<MapLayer*> QueryAnalyzer::execute()
{
    QList<MapLayer*> results;
    auto* qp = dynamic_cast<QueryParam*>(mParams.get());
    if (!qp || mInputLayers.isEmpty()) return results;

    VectorLayer* source = dynamic_cast<VectorLayer*>(mInputLayers.first());
    if (!source) return results;

    VectorLayer* result = nullptr;
    QString outputName = qp->outputLayerName.isEmpty()
        ? QString("%1_QueryResult").arg(source->name())
        : qp->outputLayerName;

    switch (qp->queryType)
    {
    case QueryParam::AttributeQuery:
        result = filterByAttribute(source, qp->attributeFilter, outputName);
        break;

    case QueryParam::SpatialQuery:
    {
        VectorLayer* refLayer = nullptr;
        if (mInputLayers.size() >= 2)
            refLayer = dynamic_cast<VectorLayer*>(mInputLayers[1]);
        if (refLayer)
            result = filterBySpatial(source, refLayer, qp->spatialRelation, outputName);
        break;
    }

    case QueryParam::CombinedQuery:
        if (mInputLayers.size() >= 2 && qp->bufferDistance > 0)
        {
            VectorLayer* refLayer = dynamic_cast<VectorLayer*>(mInputLayers[1]);
            if (refLayer)
                result = bufferAndFilter(source, refLayer, qp->bufferDistance, outputName);
        }
        break;
    }

    if (result) results.append(result);
    return results;
}

VectorLayer* QueryAnalyzer::filterByAttribute(VectorLayer* source,
                                               const QString& expression,
                                               const QString& outputName)
{
    if (!source || !source->qgsVectorLayer()) return nullptr;
    QgsVectorLayer* srcLayer = source->qgsVectorLayer();

    // 使用QGIS选择
    QgsFeatureRequest request;
    request.setFilterExpression(expression);

    // 创建内存图层
    QString uri = QString("%1?crs=%2")
        .arg(QgsWkbTypes::displayString(srcLayer->wkbType()), srcLayer->crs().authid());
    auto memLayer = new QgsVectorLayer(uri, outputName, "memory");
    memLayer->dataProvider()->addAttributes(srcLayer->fields().toList());
    memLayer->updateFields();
    memLayer->startEditing();

    QgsFeatureIterator it = srcLayer->getFeatures(request);
    QgsFeature feat;
    while (it.nextFeature(feat))
    {
        memLayer->addFeature(feat);
    }

    memLayer->commitChanges();

    // 写入临时文件
    QString tmpPath = QDir::tempPath() + "/geoai_query_" +
                      QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8) + ".gpkg";

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = "GPKG";
    options.fileEncoding = "UTF-8";
    QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, options);

    delete memLayer;

    auto resultLayer = new VectorLayer(outputName);
    auto ds = std::make_unique<FileDataSource>(tmpPath);
    resultLayer->setDataSource(std::move(ds));
    resultLayer->load();

    // 高亮渲染
    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(255, 255, 0, 180));
    renderer->setStrokeColor(QColor(255, 150, 0));
    renderer->setLineWidth(2.0);
    resultLayer->setRenderer(std::move(renderer));
    resultLayer->applyRenderer();

    return resultLayer;
}

VectorLayer* QueryAnalyzer::filterBySpatial(VectorLayer* source, VectorLayer* reference,
                                              const QString& relation,
                                              const QString& outputName)
{
    if (!source || !reference) return nullptr;
    QgsVectorLayer* srcLayer = source->qgsVectorLayer();
    QgsVectorLayer* refLayer = reference->qgsVectorLayer();
    if (!srcLayer || !refLayer) return nullptr;

    if (!srcLayer->crs().isValid()) srcLayer->setCrs(QgsCoordinateReferenceSystem("EPSG:4326"));
    if (!refLayer->crs().isValid()) refLayer->setCrs(QgsCoordinateReferenceSystem("EPSG:4326"));

    auto memLayer = new QgsVectorLayer(
        QString("%1?crs=%2")
            .arg(QgsWkbTypes::displayString(srcLayer->wkbType()), srcLayer->crs().authid()),
        outputName, "memory");

    memLayer->dataProvider()->addAttributes(srcLayer->fields().toList());
    memLayer->updateFields();
    memLayer->startEditing();

    QgsFeatureIterator it = srcLayer->getFeatures();
    QgsFeature feat;
    while (it.nextFeature(feat))
    {
        if (!feat.hasGeometry()) continue;

        // 检查与参考图层的关系
        QgsFeatureRequest req;
        req.setFilterRect(feat.geometry().boundingBox());
        QgsFeatureIterator refIt = refLayer->getFeatures(req);
        QgsFeature refFeat;
        bool matched = false;

        while (refIt.nextFeature(refFeat))
        {
            if (!refFeat.hasGeometry()) continue;

            if (relation == "intersects" &&
                feat.geometry().intersects(refFeat.geometry()))
            {
                matched = true;
                break;
            }
            else if (relation == "contains" &&
                     feat.geometry().contains(refFeat.geometry()))
            {
                matched = true;
                break;
            }
            else if (relation == "within" &&
                     feat.geometry().within(refFeat.geometry()))
            {
                matched = true;
                break;
            }
        }

        if (matched)
            memLayer->addFeature(feat);
    }

    memLayer->commitChanges();

    QString tmpPath = QDir::tempPath() + "/geoai_spatial_query_" +
                      QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8) + ".gpkg";

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = "GPKG";
    options.fileEncoding = "UTF-8";
    QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, options);

    delete memLayer;

    auto resultLayer = new VectorLayer(outputName);
    auto ds = std::make_unique<FileDataSource>(tmpPath);
    resultLayer->setDataSource(std::move(ds));
    resultLayer->load();

    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(0, 255, 255, 150));
    renderer->setLineWidth(2.0);
    resultLayer->setRenderer(std::move(renderer));
    resultLayer->applyRenderer();

    return resultLayer;
}

VectorLayer* QueryAnalyzer::bufferAndFilter(VectorLayer* target, VectorLayer* reference,
                                              double distance, const QString& outputName)
{
    if (!target || !reference) return nullptr;
    QgsVectorLayer* tgtLayer = target->qgsVectorLayer();
    QgsVectorLayer* refLayer = reference->qgsVectorLayer();
    if (!tgtLayer || !refLayer) return nullptr;

    auto memLayer = new QgsVectorLayer(
        QString("Polygon?crs=%1").arg(tgtLayer->crs().authid()),
        outputName, "memory");

    memLayer->dataProvider()->addAttributes(tgtLayer->fields().toList());
    memLayer->updateFields();
    memLayer->startEditing();

    // Buffer each reference feature, then check target intersection
    QgsFeatureIterator refIt = refLayer->getFeatures();
    QgsFeature refFeat;
    QList<QgsGeometry> buffers;
    while (refIt.nextFeature(refFeat))
    {
        if (!refFeat.hasGeometry()) continue;
        QgsGeometry buf = refFeat.geometry().buffer(distance, 8);
        if (!buf.isNull())
            buffers.append(buf);
    }

    if (buffers.isEmpty()) { delete memLayer; return nullptr; }

    qDebug() << "[BufferFilter] Buffers:" << buffers.size() << "distance:" << distance;
    int matched = 0;
    QgsFeatureIterator tgtIt = tgtLayer->getFeatures();
    QgsFeature tgtFeat;
    while (tgtIt.nextFeature(tgtFeat))
    {
        if (!tgtFeat.hasGeometry()) continue;
        for (const QgsGeometry& buf : buffers)
        {
            if (tgtFeat.geometry().intersects(buf))
            {
                memLayer->addFeature(tgtFeat);
                matched++;
                break;
            }
        }
    }

    memLayer->commitChanges();

    qDebug() << "[BufferFilter] Matched:" << matched << "of" << tgtLayer->featureCount() << "target features";

    if (matched == 0) { delete memLayer; return nullptr; }

    QString tmpPath = QDir::tempPath() + "/geoai_buffer_filter_" +
                      QUuid::createUuid().toString(QUuid::WithoutBraces).mid(1, 8) + ".gpkg";

    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = "GPKG";
    options.fileEncoding = "UTF-8";
    QgsVectorFileWriter::writeAsVectorFormat(memLayer, tmpPath, options);

    delete memLayer;

    auto resultLayer = new VectorLayer(outputName);
    auto ds = std::make_unique<FileDataSource>(tmpPath);
    resultLayer->setDataSource(std::move(ds));
    resultLayer->load();

    auto renderer = std::make_unique<SimpleRenderer>();
    renderer->setFillColor(QColor(255, 50, 50, 180));
    renderer->setLineWidth(2.0);
    resultLayer->setRenderer(std::move(renderer));
    resultLayer->applyRenderer();

    return resultLayer;
}


// ============================================================
// 从 AnalysisCommand 创建 QueryAnalyzer
// ============================================================

std::unique_ptr<QueryAnalyzer> QueryAnalyzer::fromCommand(const AnalysisCommand& cmd)
{
    auto analyzer = std::make_unique<QueryAnalyzer>();

    auto params = std::make_unique<QueryParam>();

    switch (cmd.type)
    {
    case AnalysisType::AttributeQuery:
        params->queryType = QueryParam::AttributeQuery;
        params->attributeFilter = cmd.attributeFilter;
        break;

    case AnalysisType::SpatialQuery:
        params->queryType = QueryParam::SpatialQuery;
        params->spatialRelation = AnalysisCommand::spatialRelationToString(cmd.relation);
        break;

    case AnalysisType::BufferQuery:
        params->queryType = QueryParam::CombinedQuery;
        // BufferQuery会走 BufferAnalyzer + QueryAnalyzer的组合
        // 这里先存储参数，后续可扩展
        break;

    default:
        break;
    }

    params->layerId = cmd.targetLayer;
    params->referenceLayerId = cmd.referenceLayer;
    params->outputLayerName = cmd.targetLayer + "_QueryResult";

    analyzer->setParameters(std::move(params));
    return analyzer;
}

#pragma warning(pop)
