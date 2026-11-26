#pragma once

#include "QueryParser.h"

// ============================================================
// LLMParser — 大模型增强NL解析器（加分项）
//   OOP: QueryParser 派生类，为LLM接入预留扩展点
//   通过HTTP API调用大模型，将自然语言转为JSON分析指令
// ============================================================
class LLMParser : public QueryParser
{
public:
    LLMParser();

    // 配置API
    void setApiEndpoint(const QString& url);
    void setApiKey(const QString& key);
    void setModel(const QString& model);

    AnalysisCommand parse(const QString& text) override;
    QString parserName() const override { return "LLMParser"; }
    QString lastReply() const { return mLastReply; }

    // Set context: loaded layer names
    void setLayerContext(const QStringList& layerNames);

    // Explain analysis results (second LLM call)
    QString explainResults(const QString& userQuery, const QString& resultSummary);

private:
    QString mApiEndpoint;
    QString mApiKey;
    QString mModel = "deepseek-v4-pro";
    QString mLastReply;
    QString mLayerContext;

    QString buildPrompt(const QString& userInput) const;

    // 发送HTTP请求并解析响应（使用Qt Network或简单的curl封装）
    QString sendRequest(const QString& prompt) const;

    // 解析LLM返回的JSON
    AnalysisCommand parseResponse(const QString& jsonStr) const;
};
