#include "RuleBasedParser.h"
#include <QRegularExpressionMatch>

RuleBasedParser::RuleBasedParser()
{
    initDefaultKeywords();
}

void RuleBasedParser::initDefaultKeywords()
{
    // Spatial relation indicators
    mTypeMap["距离"] = AnalysisType::BufferQuery;
    mTypeMap["米内"] = AnalysisType::BufferQuery;
    mTypeMap["周边"] = AnalysisType::BufferQuery;
    mTypeMap["附近"] = AnalysisType::BufferQuery;

    mTypeMap["大于"] = AnalysisType::AttributeQuery;
    mTypeMap["小于"] = AnalysisType::AttributeQuery;

    mTypeMap["相交"] = AnalysisType::SpatialQuery;
    mTypeMap["包含"] = AnalysisType::SpatialQuery;
    mTypeMap["重叠"] = AnalysisType::SpatialQuery;

    mTypeMap["交集"] = AnalysisType::OverlayAnalysis;
    mTypeMap["并集"] = AnalysisType::OverlayAnalysis;
    mTypeMap["叠加"] = AnalysisType::OverlayAnalysis;
}

void RuleBasedParser::addKeyword(const QString& key, const QString& value)
{
    mKeywords[key] = value;
}

void RuleBasedParser::removeKeyword(const QString& key)
{
    mKeywords.remove(key);
}

AnalysisCommand RuleBasedParser::parse(const QString& text)
{
    if (text.trimmed().isEmpty())
        return AnalysisCommand();

    QString t = text.simplified();

    // ---- Priority order: Buffer > Overlay > Attribute > Spatial ----

    // 1. Buffer: "距离X米内" / "X周边"
    if (rxMatch(t, "距离|米内|周边|附近|公里内|范围内"))
    {
        return parseBufferQuery(t);
    }

    // 2. Overlay: "X与Y的交集/并集"
    if (rxMatch(t, "交集|并集|叠加|分析.*与"))
    {
        return parseOverlayAnalysis(t);
    }

    // 3. Attribute: "AREA_MI > 100" / "人口大于" / "查询大于"
    if (rxMatch(t, "大于|小于|等于|>=|<=|!=|\\w+\\s*[><=]"))
    {
        return parseAttributeQuery(t);
    }

    // 4. Spatial: "X与Y相交" / "X在Y境内" / "X穿过Y" / "X包含Y"
    if (rxMatch(t, "相交|包含|重叠|位于|穿过|境内|内部|里[面边]"))
    {
        return parseSpatialQuery(t);
    }

    // 5. Default: try spatial query (most flexible)
    return parseSpatialQuery(t);
}

bool RuleBasedParser::rxMatch(const QString& text, const QString& pattern)
{
    QRegularExpression re(pattern);
    return re.isValid() && re.match(text).hasMatch();
}

// ============================================================
// Buffer query
// ============================================================
AnalysisCommand RuleBasedParser::parseBufferQuery(const QString& text)
{
    AnalysisCommand cmd(AnalysisType::BufferQuery);
    cmd.distance = extractDistance(text);
    if (cmd.distance <= 0) cmd.distance = 100.0;
    cmd.distanceUnit = extractDistanceUnit(text);

    // Extract layers: "距离X Y米内的Z" or "X周边Y米内的Z"
    auto layers = extractTwoLayers(text, QStringList()
        << "距离" << "米内" << "米以内" << "公里内" << "范围内" << "周边" << "附近");
    cmd.referenceLayer = layers.first;
    cmd.targetLayer = layers.second;

    return cmd;
}

// ============================================================
// Attribute query
// ============================================================
AnalysisCommand RuleBasedParser::parseAttributeQuery(const QString& text)
{
    AnalysisCommand cmd(AnalysisType::AttributeQuery);

    // Try standard field comparison: "AREA_MI > 100" or "人口大于500"
    QRegularExpression re1("(\\w+)\\s*(大于|小于|等于|>=|<=|!=|>|<|=)\\s*(\\S+)");
    auto m1 = re1.match(text);
    if (m1.hasMatch()) {
        QString field = m1.captured(1);
        QString op = m1.captured(2);
        if (op == ">") op = ">";
        else if (op == "<") op = "<";
        else if (op == "=") op = "=";
        else if (op == "大于") op = ">";
        else if (op == "小于") op = "<";
        else if (op == "等于") op = "=";
        else if (op == ">=" || op == "不小于") op = ">=";
        else if (op == "<=" || op == "不大于") op = "<=";
        else if (op == "!=" || op == "不等于") op = "!=";
        cmd.attributeFilter = QString("%1 %2 %3").arg(field, op, m1.captured(3));
    }
    else {
        // Chinese pattern: "人口大于500万的"
        QRegularExpression re2("(\\S+)\\s*(大于|小于|等于)\\s*(\\d+)");
        auto m2 = re2.match(text);
        if (m2.hasMatch()) {
            cmd.attributeFilter = QString("%1 %2 %3")
                .arg(m2.captured(1), m2.captured(2) == QString("大于") ? ">" : 
                                   m2.captured(2) == QString("小于") ? "<" : "=",
                     m2.captured(3));
        }
    }

    // Extract target layer
    cmd.targetLayer = extractLastWord(text, QStringList()
        << "查询" << "找出" << "查找" << "的" << "数据" << "所有" << "什么" << "哪些");

    return cmd;
}

// ============================================================
// Spatial query
// ============================================================
AnalysisCommand RuleBasedParser::parseSpatialQuery(const QString& text)
{
    AnalysisCommand cmd(AnalysisType::SpatialQuery);

    // Detect relation
    if (rxMatch(text, "境内|内部|里[面边]|位于.*内|在.*内|都在|在.*里面"))
        cmd.relation = SpatialRelation::Within;
    else if (rxMatch(text, "包含|包括|包[含括]了"))
        cmd.relation = SpatialRelation::Contains;
    else if (rxMatch(text, "穿过|跨越|经过|横穿"))
        cmd.relation = SpatialRelation::Intersects;
    else if (rxMatch(text, "重叠|覆[盖叠]"))
        cmd.relation = SpatialRelation::Overlaps;
    else if (rxMatch(text, "相邻|接[壤触]|交界|连[接着]"))
        cmd.relation = SpatialRelation::Touches;
    else
        cmd.relation = SpatialRelation::Intersects;

    // Extract layers: "X在Y境内" / "X与Y相交" / "X穿过Y"
    // Pattern 1: "X在Y境内" → reference=Y, target=X
    QRegularExpression re1("(.+?)在(.+?)(境内|内部|里[面边]|里面)");
    auto m1 = re1.match(text);
    if (m1.hasMatch()) {
        cmd.targetLayer = cleanLayerName(m1.captured(1));
        cmd.referenceLayer = cleanLayerName(m1.captured(2));
        return cmd;
    }

    // Pattern 2: "与X相交的Y"
    QRegularExpression re2("与(.+?)(相交|包含|重叠|穿过)");
    auto m2 = re2.match(text);
    if (m2.hasMatch()) {
        cmd.referenceLayer = cleanLayerName(m2.captured(1));
        cmd.targetLayer = extractLastWord(text, QStringList()
            << "与" << "相交" << "包含" << "重叠" << "穿过" << "的" << "显示" << "查找");
        return cmd;
    }

    // Pattern 3: "X包含Y" → target=X, reference=Y
    QRegularExpression re3("(.+?)包含(.+?)$");
    auto m3 = re3.match(text);
    if (m3.hasMatch()) {
        cmd.targetLayer = cleanLayerName(m3.captured(1));
        cmd.referenceLayer = cleanLayerName(m3.captured(2));
        return cmd;
    }

    // Pattern 4: "X穿过Y" → target=X, reference=Y
    QRegularExpression re4("(.+?)穿过(.+?)$");
    auto m4 = re4.match(text);
    if (m4.hasMatch()) {
        cmd.targetLayer = cleanLayerName(m4.captured(1));
        cmd.referenceLayer = cleanLayerName(m4.captured(2));
        return cmd;
    }

    // Fallback: extract two words as layers
    auto layers = extractTwoLayers(text, QStringList()
        << "的" << "显示" << "查找" << "找出" << "与" << "和" << "在" << "境内" << "内部");
    cmd.targetLayer = layers.first;
    cmd.referenceLayer = layers.second;

    return cmd;
}

// ============================================================
// Overlay analysis
// ============================================================
AnalysisCommand RuleBasedParser::parseOverlayAnalysis(const QString& text)
{
    AnalysisCommand cmd(AnalysisType::OverlayAnalysis);

    if (rxMatch(text, "交集|相[交切]"))
        cmd.overlayType = OverlayType::Intersection;
    else if (rxMatch(text, "并集|联合|合并"))
        cmd.overlayType = OverlayType::Union;
    else
        cmd.overlayType = OverlayType::Intersection;

    QRegularExpression re("(.+?)与(.+?)(的|叠加|交集|并集|分析|$)");
    auto m = re.match(text);
    if (m.hasMatch()) {
        cmd.targetLayer = cleanLayerName(m.captured(1));
        cmd.referenceLayer = cleanLayerName(m.captured(2));
    }

    return cmd;
}

// ============================================================
// Helpers
// ============================================================

double RuleBasedParser::extractDistance(const QString& text)
{
    QRegularExpression re("(\\d+)\\s*(米|公里|千米|m|km)");
    auto m = re.match(text);
    if (m.hasMatch()) {
        double d = m.captured(1).toDouble();
        QString u = m.captured(2);
        if (u == "公里" || u == "千米" || u == "km") d *= 1000.0;
        return d;
    }
    return 0.0;
}

QString RuleBasedParser::extractDistanceUnit(const QString& text)
{
    QRegularExpression re("\\d+\\s*(米|公里|千米|m|km)");
    auto m = re.match(text);
    if (m.hasMatch()) {
        QString u = m.captured(1);
        if (u == "m") return "米";
        if (u == "km") return "千米";
        return u;
    }
    return "米";
}

SpatialRelation RuleBasedParser::extractSpatialRelation(const QString& text)
{
    if (rxMatch(text, "境内|内部|里[面边]|在.*内|都在"))
        return SpatialRelation::Within;
    if (rxMatch(text, "包含"))
        return SpatialRelation::Contains;
    if (rxMatch(text, "相交|穿过|交叉"))
        return SpatialRelation::Intersects;
    if (rxMatch(text, "重叠"))
        return SpatialRelation::Overlaps;
    if (rxMatch(text, "相邻|接触"))
        return SpatialRelation::Touches;
    return SpatialRelation::Intersects;
}

QPair<QString, QString> RuleBasedParser::extractTwoLayers(const QString& text, const QStringList& exclude)
{
    QString cleaned = text;
    for (const QString& w : exclude)
        cleaned.replace(w, " ");
    cleaned.replace(QRegularExpression("[\\d.,，。！？、；：（）()\\[\\]【】\"\"'']"), " ");

    QStringList words = cleaned.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    QPair<QString, QString> result;
    // Return two meaningful words (last two non-trivial words)
    for (int i = words.size() - 1; i >= 0; --i) {
        QString w = words[i].trimmed();
        if (w.length() >= 2 && !w[0].isDigit()) {
            if (result.second.isEmpty()) result.second = w;
            else { result.first = w; break; }
        }
    }
    return result;
}

QString RuleBasedParser::extractLastWord(const QString& text, const QStringList& exclude)
{
    QString cleaned = text;
    for (const QString& w : exclude)
        cleaned.replace(w, " ");
    cleaned.replace(QRegularExpression("[\\d.,，。！？、；：（）()\\[\\]【】\"\"'']"), " ");

    QStringList words = cleaned.split(QRegularExpression("\\s+"), QString::SkipEmptyParts);
    for (int i = words.size() - 1; i >= 0; --i) {
        QString w = words[i].trimmed();
        if (w.length() >= 2 && !w[0].isDigit())
            return w;
    }
    return QString();
}

QString RuleBasedParser::cleanLayerName(const QString& name)
{
    QString n = name.trimmed();
    n.remove(QRegularExpression("[，。！？、；：（）()\"\"''\\[\\]【】]$"));
    n.remove(QRegularExpression("[都还也]?[是的有在]?$"));
    n.remove(QRegularExpression("吗$"));
    return n;
}

QString RuleBasedParser::normalizeLayerName(const QString& name)
{
    QString n = name;
    n.remove(QRegularExpression("[^\\w\\u4e00-\\u9fff]"));
    if (mKeywords.contains(n)) return mKeywords[n];
    return n;
}
