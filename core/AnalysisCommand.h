#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

// ============================================================
// AnalysisCommand — 结构化分析指令（命令模式）
//   OOP: 命令模式 — 将NL解析结果封装为可执行、可序列化的命令
//   是 QueryParser 和 SpatialAnalyzer 之间的数据传输对象
// ============================================================

// 分析类型枚举
enum class AnalysisType
{
    None,
    BufferQuery,        // 缓冲区查询: "距离X米内的Y"
    AttributeQuery,     // 属性查询: "人口大于X的Y"
    SpatialQuery,       // 空间关系查询: "与X相交/包含的Y"
    OverlayAnalysis,    // 叠加分析: "X与Y的交集"
    BufferAnalysis,     // 纯缓冲区生成
    Unknown
};

// 空间关系枚举
enum class SpatialRelation
{
    Intersects,
    Contains,
    Within,
    Touches,
    Crosses,
    Overlaps,
    Disjoint,
    Near
};

// 叠加类型枚举
enum class OverlayType
{
    Intersection,
    Union,
    Difference,
    SymDifference
};


class AnalysisCommand
{
public:
    AnalysisCommand();
    explicit AnalysisCommand(AnalysisType type);

    // —— 核心属性 ——
    AnalysisType type = AnalysisType::None;
    QString targetLayer;        // 目标图层名
    QString referenceLayer;     // 参考图层名
    double distance = 0.0;      // 距离（缓冲区查询用）
    QString distanceUnit = "米";  // 距离单位
    QString attributeFilter;    // 属性过滤条件（如 "人口 > 500000"）
    SpatialRelation relation = SpatialRelation::Intersects;
    OverlayType overlayType = OverlayType::Intersection;

    // —— 序列化 ——
    QJsonObject toJson() const;
    static AnalysisCommand fromJson(const QJsonObject& json);
    static AnalysisCommand fromJsonString(const QString& jsonStr);

    // —— 描述 ——
    QString description() const;
    bool isValid() const;

    // 公开静态方法：空间关系转字符串
    static QString spatialRelationToString(SpatialRelation r);

private:
    static QString analysisTypeToString(AnalysisType t);
    static AnalysisType stringToAnalysisType(const QString& s);
    static QString overlayTypeToString(OverlayType t);
    static OverlayType stringToOverlayType(const QString& s);
    static SpatialRelation stringToSpatialRelation(const QString& s);
};
