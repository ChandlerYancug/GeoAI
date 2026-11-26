#pragma once

#include "QueryParser.h"
#include <QMap>
#include <QPair>
#include <QStringList>
#include <QRegularExpression>

// ============================================================
// RuleBasedParser — 基于规则的NL解析器
//   OOP: QueryParser 派生类
//   使用正则表达式 + 关键词词典实现基础语义解析
//
//   支持的查询模式：
//   - 缓冲区查询: "找出距离X Y米内的Z"
//   - 属性查询:   "查询人口大于X的Y"
//   - 空间查询:   "显示与X相交的Y"
//   - 叠加分析:   "分析X与Y的交集"
// ============================================================
class RuleBasedParser : public QueryParser
{
public:
    RuleBasedParser();

    AnalysisCommand parse(const QString& text) override;
    QString parserName() const override { return "RuleBasedParser"; }

    // 添加自定义关键词
    void addKeyword(const QString& key, const QString& value);
    void removeKeyword(const QString& key);

private:
    // 关键词词典
    QMap<QString, QString> mKeywords;       // 关键词 → 含义映射
    QMap<QString, AnalysisType> mTypeMap;   // 模式词 → 分析类型

    // 解析子方法
    AnalysisCommand parseBufferQuery(const QString& text);
    AnalysisCommand parseAttributeQuery(const QString& text);
    AnalysisCommand parseSpatialQuery(const QString& text);
    AnalysisCommand parseOverlayAnalysis(const QString& text);

    // 辅助
    bool rxMatch(const QString& text, const QString& pattern);
    double extractDistance(const QString& text);
    QString extractDistanceUnit(const QString& text);
    SpatialRelation extractSpatialRelation(const QString& text);
    QPair<QString, QString> extractTwoLayers(const QString& text, const QStringList& exclude);
    QString extractLastWord(const QString& text, const QStringList& exclude);
    QString cleanLayerName(const QString& name);
    QString normalizeLayerName(const QString& name);

    void initDefaultKeywords();
};
