#pragma once

#include "SpatialAnalyzer.h"

// ============================================================
// BufferAnalyzer — 缓冲区分析器
//   OOP: SpatialAnalyzer 派生类，策略模式
//   使用QGIS API执行缓冲区生成
// ============================================================
class BufferAnalyzer : public SpatialAnalyzer
{
    Q_OBJECT

public:
    explicit BufferAnalyzer(QObject* parent = nullptr);
    ~BufferAnalyzer() override;

    QList<MapLayer*> execute() override;
    QString description() const override;
    QString analyzerType() const override { return "BufferAnalyzer"; }

private:
    VectorLayer* createBufferLayer(VectorLayer* source, double distance, int segments);
};


// ============================================================
// OverlayAnalyzer — 叠加分析器
//   实现Intersection / Union操作
// ============================================================
class OverlayAnalyzer : public SpatialAnalyzer
{
    Q_OBJECT

public:
    explicit OverlayAnalyzer(QObject* parent = nullptr);
    ~OverlayAnalyzer() override;

    QList<MapLayer*> execute() override;
    QString description() const override;
    QString analyzerType() const override { return "OverlayAnalyzer"; }

private:
    VectorLayer* performIntersection(VectorLayer* layerA, VectorLayer* layerB,
                                      const QString& outputName);
    VectorLayer* performUnion(VectorLayer* layerA, VectorLayer* layerB,
                               const QString& outputName);
};


// ============================================================
// QueryAnalyzer — 属性查询与空间查询分析器
//   按属性条件或空间关系筛选要素
// ============================================================
class QueryAnalyzer : public SpatialAnalyzer
{
    Q_OBJECT

public:
    explicit QueryAnalyzer(QObject* parent = nullptr);
    ~QueryAnalyzer() override;

    QList<MapLayer*> execute() override;
    QString description() const override;
    QString analyzerType() const override { return "QueryAnalyzer"; }

    // 便捷方法：直接从command创建
    static std::unique_ptr<QueryAnalyzer> fromCommand(const class AnalysisCommand& cmd);

private:
    VectorLayer* filterByAttribute(VectorLayer* source, const QString& expression,
                                    const QString& outputName);
    VectorLayer* filterBySpatial(VectorLayer* source, VectorLayer* reference,
                                  const QString& relation, const QString& outputName);
    VectorLayer* bufferAndFilter(VectorLayer* target, VectorLayer* reference,
                                  double distance, const QString& outputName);
};
