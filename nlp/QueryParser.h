#pragma once

#include <QString>
#include <QObject>
#include <memory>
#include "../core/AnalysisCommand.h"

// ============================================================
// QueryParser — 自然语言查询解析器抽象接口
//   OOP: 策略模式/抽象工厂 — 不同解析实现可互换
//   UI层只依赖此抽象接口
// ============================================================
class QueryParser
{
public:
    virtual ~QueryParser() = default;

    // 解析自然语言文本，返回结构化指令
    virtual AnalysisCommand parse(const QString& text) = 0;

    // 解析器名称
    virtual QString parserName() const = 0;

    // 是否支持该查询类型
    virtual bool canParse(const QString& text) const;

    // 工厂方法
    enum ParserType { RuleBased, LLM };
    static std::unique_ptr<QueryParser> create(ParserType type);
};
