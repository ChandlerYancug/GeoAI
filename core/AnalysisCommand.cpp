#include "AnalysisCommand.h"
#include <QJsonArray>

AnalysisCommand::AnalysisCommand() = default;
AnalysisCommand::AnalysisCommand(AnalysisType type) : type(type) {}

bool AnalysisCommand::isValid() const
{
    if (type == AnalysisType::None || type == AnalysisType::Unknown)
        return false;

    switch (type)
    {
    case AnalysisType::BufferQuery:
    case AnalysisType::BufferAnalysis:
        return !targetLayer.isEmpty() && distance > 0;

    case AnalysisType::AttributeQuery:
        return !targetLayer.isEmpty();  // attributeFilter can be empty (select all)

    case AnalysisType::SpatialQuery:
        return !targetLayer.isEmpty() && !referenceLayer.isEmpty();

    case AnalysisType::OverlayAnalysis:
        return !targetLayer.isEmpty() && !referenceLayer.isEmpty();

    default:
        return false;
    }
}

QString AnalysisCommand::description() const
{
    switch (type)
    {
    case AnalysisType::BufferQuery:
        return QString("找出距离%1%2%3内的%4")
            .arg(referenceLayer)
            .arg(distance)
            .arg(distanceUnit)
            .arg(targetLayer);

    case AnalysisType::AttributeQuery:
        return QString("在%1中查询: %2").arg(targetLayer, attributeFilter);

    case AnalysisType::SpatialQuery:
        return QString("查询与%1%2的%3")
            .arg(referenceLayer)
            .arg(spatialRelationToString(relation))
            .arg(targetLayer);

    case AnalysisType::OverlayAnalysis:
        return QString("%1 与 %2 的%3分析")
            .arg(targetLayer)
            .arg(referenceLayer)
            .arg(overlayTypeToString(overlayType));

    case AnalysisType::BufferAnalysis:
        return QString("对%1生成%2%3缓冲区")
            .arg(targetLayer)
            .arg(distance)
            .arg(distanceUnit);

    default:
        return "未知分析";
    }
}

// —— JSON序列化 ——

QJsonObject AnalysisCommand::toJson() const
{
    QJsonObject obj;
    obj["type"] = analysisTypeToString(type);
    obj["targetLayer"] = targetLayer;
    obj["referenceLayer"] = referenceLayer;
    obj["distance"] = distance;
    obj["distanceUnit"] = distanceUnit;
    obj["attributeFilter"] = attributeFilter;
    obj["relation"] = spatialRelationToString(relation);
    obj["overlayType"] = overlayTypeToString(overlayType);
    return obj;
}

AnalysisCommand AnalysisCommand::fromJson(const QJsonObject& json)
{
    AnalysisCommand cmd;
    cmd.type = stringToAnalysisType(json["type"].toString());
    cmd.targetLayer = json["targetLayer"].toString();
    cmd.referenceLayer = json["referenceLayer"].toString();
    cmd.distance = json["distance"].toDouble();
    cmd.distanceUnit = json["distanceUnit"].toString("米");
    cmd.attributeFilter = json["attributeFilter"].toString();
    cmd.relation = stringToSpatialRelation(json["relation"].toString());
    cmd.overlayType = stringToOverlayType(json["overlayType"].toString());
    return cmd;
}

AnalysisCommand AnalysisCommand::fromJsonString(const QString& jsonStr)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    return fromJson(doc.object());
}


// —— 枚举 ↔ 字符串转换 ——

QString AnalysisCommand::analysisTypeToString(AnalysisType t)
{
    switch (t)
    {
    case AnalysisType::BufferQuery:     return "buffer_query";
    case AnalysisType::AttributeQuery:  return "attribute_query";
    case AnalysisType::SpatialQuery:    return "spatial_query";
    case AnalysisType::OverlayAnalysis: return "overlay_analysis";
    case AnalysisType::BufferAnalysis:  return "buffer_analysis";
    case AnalysisType::Unknown:         return "unknown";
    default:                            return "none";
    }
}

AnalysisType AnalysisCommand::stringToAnalysisType(const QString& s)
{
    if (s == "buffer_query")      return AnalysisType::BufferQuery;
    if (s == "attribute_query")   return AnalysisType::AttributeQuery;
    if (s == "spatial_query")     return AnalysisType::SpatialQuery;
    if (s == "overlay_analysis")  return AnalysisType::OverlayAnalysis;
    if (s == "buffer_analysis")   return AnalysisType::BufferAnalysis;
    return AnalysisType::None;
}

QString AnalysisCommand::spatialRelationToString(SpatialRelation r)
{
    switch (r)
    {
    case SpatialRelation::Intersects:   return "intersects";
    case SpatialRelation::Contains:     return "contains";
    case SpatialRelation::Within:       return "within";
    case SpatialRelation::Touches:      return "touches";
    case SpatialRelation::Crosses:      return "crosses";
    case SpatialRelation::Overlaps:     return "overlaps";
    case SpatialRelation::Disjoint:     return "disjoint";
    case SpatialRelation::Near:         return "near";
    default:                            return "intersects";
    }
}

SpatialRelation AnalysisCommand::stringToSpatialRelation(const QString& s)
{
    if (s == "intersects")  return SpatialRelation::Intersects;
    if (s == "contains")    return SpatialRelation::Contains;
    if (s == "within")      return SpatialRelation::Within;
    if (s == "touches")     return SpatialRelation::Touches;
    if (s == "crosses")     return SpatialRelation::Crosses;
    if (s == "overlaps")    return SpatialRelation::Overlaps;
    if (s == "disjoint")    return SpatialRelation::Disjoint;
    if (s == "near")        return SpatialRelation::Near;
    return SpatialRelation::Intersects;
}

QString AnalysisCommand::overlayTypeToString(OverlayType t)
{
    switch (t)
    {
    case OverlayType::Intersection:    return "intersection";
    case OverlayType::Union:           return "union";
    case OverlayType::Difference:      return "difference";
    case OverlayType::SymDifference:   return "sym_difference";
    default:                           return "intersection";
    }
}

OverlayType AnalysisCommand::stringToOverlayType(const QString& s)
{
    if (s == "intersection")    return OverlayType::Intersection;
    if (s == "union")           return OverlayType::Union;
    if (s == "difference")      return OverlayType::Difference;
    if (s == "sym_difference")  return OverlayType::SymDifference;
    return OverlayType::Intersection;
}


