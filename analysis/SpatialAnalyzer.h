#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include <qgsrectangle.h>
#include "../core/MapLayer.h"

// 前向声明
class AnalysisParam;

// ============================================================
// SpatialAnalyzer — 空间分析器抽象基类（策略模式）
//   OOP: 策略模式 — 不同分析算法可互换
//   OOP: 命令模式 — execute() 执行具体分析命令
//   上层UI只需创建具体分析器并调用 execute()
// ============================================================
class SpatialAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit SpatialAnalyzer(QObject* parent = nullptr);
    virtual ~SpatialAnalyzer();

    // 执行分析，返回新生成的图层
    virtual QList<MapLayer*> execute() = 0;

    // 设置分析参数
    void setParameters(std::unique_ptr<AnalysisParam> params);
    AnalysisParam* parameters() const;

    // 描述
    virtual QString description() const = 0;
    virtual QString analyzerType() const = 0;

    // 设置输入图层
    void setInputLayers(const QList<MapLayer*>& layers);
    QList<MapLayer*> inputLayers() const;

signals:
    void progressUpdated(int percent);
    void finished();

protected:
    std::unique_ptr<AnalysisParam> mParams;
    QList<MapLayer*> mInputLayers;
};


// ============================================================
// AnalysisParam — 分析参数抽象基类
//   各类分析参数由此派生
// ============================================================
class AnalysisParam
{
public:
    virtual ~AnalysisParam() = default;
    virtual QString typeName() const = 0;
};


// ============================================================
// BufferParam — 缓冲区分析参数
// ============================================================
class BufferParam : public AnalysisParam
{
public:
    QString typeName() const override { return "Buffer"; }

    QString targetLayerId;
    double distance = 100.0;
    QString distanceUnit = "m";
    int segments = 8;
    bool dissolve = true;
    QString outputLayerName;
};


// ============================================================
// OverlayParam — 叠加分析参数
// ============================================================
class OverlayParam : public AnalysisParam
{
public:
    enum OverlayOp { Intersection, Union, Difference, SymDifference };

    QString typeName() const override { return "Overlay"; }

    QString layerAId;
    QString layerBId;
    OverlayOp operation = Intersection;
    QString outputLayerName;
};


// ============================================================
// QueryParam — 属性/空间查询参数
// ============================================================
class QueryParam : public AnalysisParam
{
public:
    enum QueryType { AttributeQuery, SpatialQuery, CombinedQuery };

    QString typeName() const override { return "Query"; }

    QueryType queryType = AttributeQuery;
    QString layerId;
    QString attributeFilter;                // "人口 > 500000"
    QgsRectangle spatialExtent;             // 空间范围过滤
    QString referenceLayerId;               // 空间查询参考图层
    QString spatialRelation = "intersects"; // 空间关系
    QString outputLayerName;
    double bufferDistance = 0;
};
