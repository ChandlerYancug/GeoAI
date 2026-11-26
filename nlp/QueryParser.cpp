#include "QueryParser.h"
#include "RuleBasedParser.h"
#include "LLMParser.h"

bool QueryParser::canParse(const QString& text) const
{
    return !text.trimmed().isEmpty();
}

std::unique_ptr<QueryParser> QueryParser::create(ParserType type)
{
    switch (type)
    {
    case RuleBased:
        return std::make_unique<class RuleBasedParser>();
    case LLM:
        return std::make_unique<class LLMParser>();
    }
    return std::make_unique<class RuleBasedParser>();
}
