#include "LLMParser.h"
#include "RuleBasedParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>
#include <QRegularExpression>
#include <QDebug>

LLMParser::LLMParser()
{
    mApiEndpoint = "https://api.deepseek.com/chat/completions";
    mModel = "deepseek-v4-pro";
}

void LLMParser::setApiEndpoint(const QString& url) { mApiEndpoint = url; }
void LLMParser::setApiKey(const QString& key)     { mApiKey = key; }
void LLMParser::setModel(const QString& model)     { mModel = model; }

void LLMParser::setLayerContext(const QStringList& layerNames)
{
    mLayerContext = layerNames.join(", ");
}

AnalysisCommand LLMParser::parse(const QString& text)
{
    mLastReply.clear();

    if (text.trimmed().isEmpty())
        return AnalysisCommand();

    QString response = sendRequest(text);
    if (response.isEmpty())
    {
        qDebug() << "[LLM] API call failed, using rule parser";
        mLastReply = "(API调用失败，已回退规则解析)";
        RuleBasedParser fallback;
        return fallback.parse(text);
    }

    // Extract natural language reply (before JSON block)
    int jsonStart = response.indexOf("```json");
    if (jsonStart > 0)
        mLastReply = response.left(jsonStart).trimmed();
    else
        mLastReply = response;

    AnalysisCommand cmd = parseResponse(response);
    if (!cmd.isValid())
    {
        qDebug() << "[LLM] Invalid JSON from LLM, using rule parser";
        mLastReply += "\n(JSON解析失败，已回退规则解析)";
        RuleBasedParser fallback;
        return fallback.parse(text);
    }

    qDebug() << "[LLM] Success:" << cmd.description();
    return cmd;
}

QString LLMParser::explainResults(const QString& userQuery, const QString& resultSummary)
{
    if (mApiKey.isEmpty()) return QString();

    QNetworkAccessManager manager;
    QNetworkRequest req{QUrl(mApiEndpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QString("Bearer %1").arg(mApiKey).toUtf8());

    QJsonObject body;
    body["model"] = mModel;
    body["temperature"] = 0.3;
    body["max_tokens"] = 300;

    QJsonArray messages;

    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = QString(
        "You are GeoAI. The user asked a GIS question, and the system executed an analysis. "
        "Now explain the result to the user in natural language (Chinese if the query is Chinese).\n"
        "Be concise. If the result answers the question directly, say so. "
        "If there are no results found, explain that and suggest alternatives.\n"
        "User question: %1\nResult: %2"
    ).arg(userQuery, resultSummary);
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = "Please explain the result.";
    messages.append(userMsg);

    body["messages"] = messages;

    QJsonDocument doc(body);
    QNetworkReply* reply = manager.post(req, doc.toJson());

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);
    loop.exec();

    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError)
    {
        reply->deleteLater();
        return QString();
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument rDoc = QJsonDocument::fromJson(data);
    QJsonObject rObj = rDoc.object();
    QJsonArray choices = rObj["choices"].toArray();
    if (choices.isEmpty()) return QString();

    return choices[0].toObject()["message"].toObject()["content"].toString();
}

QString LLMParser::buildPrompt(const QString& userInput) const
{
    Q_UNUSED(userInput);
    return QString();
}

QString LLMParser::sendRequest(const QString& userInput) const
{
    if (mApiKey.isEmpty())
    {
        qDebug() << "[LLM] No API key configured";
        return QString();
    }

    QNetworkAccessManager manager;
    QNetworkRequest req{QUrl(mApiEndpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QString("Bearer %1").arg(mApiKey).toUtf8());

    QJsonObject body;
    body["model"] = mModel;
    body["temperature"] = 0.1;
    body["max_tokens"] = 500;

    QJsonArray messages;

    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = QString(
        "You are GeoAI, a friendly GIS analysis assistant inside a QGIS desktop application. "
        "You chat with the user in natural language, then output a JSON command for the GIS engine.\n\n"
        "RESPONSE FORMAT:\n"
        "1. First, reply to the user naturally in their language (Chinese or English)\n"
        "2. Then, on the last line, output the JSON command wrapped in ```json ... ```\n\n"
        "Example:\n"
        "User: 机场都在阿拉斯加境内吗\n"
        "You: spatial_query with within\n"
        "```json\n"
        "{\"type\":\"spatial_query\",\"targetLayer\":\"airports\",\"referenceLayer\":\"alaska\",\"relation\":\"within\"}\n"
        "```\n\n"
        "User: 距离机场100公里内的湖泊\n"
        "You: buffer_query (buffer airports 100km, find lakes within)\n"
        "```json\n"
        "{\"type\":\"buffer_query\",\"targetLayer\":\"lakes\",\"referenceLayer\":\"airports\",\"distance\":100000,\"distanceUnit\":\"meters\"}\n"
        "```\n\n"
        "CAPABILITIES (JSON types):\n"
        "- buffer_query: features within X meters of another layer\n"
        "- attribute_query: filter by attribute (field > value)\n"
        "- spatial_query: spatial relationship between layers (relation: within/intersects/contains)\n"
        "- overlay_analysis: intersection/union of two layers\n\n"
        "RULES:\n"
        "1. \"distance X 米/公里/千米 内的 Y\" = buffer_query (NOT spatial_query!)\n"
        "2. distance ALWAYS in meters (1km=1000, 1公里=1000)\n"
        "3. Use exact layer names from LOADED LAYERS list\n"
        "4. Keep replies concise, always include JSON"
    );
    if (!mLayerContext.isEmpty())
        sysMsg["content"] = sysMsg["content"].toString() + QString("\n\nLOADED LAYERS: %1").arg(mLayerContext);
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userInput;
    messages.append(userMsg);

    body["messages"] = messages;

    QJsonDocument doc(body);
    QByteArray data = doc.toJson();

    qDebug() << "[LLM] Request to:" << mApiEndpoint << "model:" << mModel;

    QNetworkReply* reply = manager.post(req, data);

    // Wait with timeout
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000); // 15 second timeout

    loop.exec();

    if (!reply->isFinished())
    {
        qDebug() << "[LLM] Request timed out";
        reply->abort();
        reply->deleteLater();
        return QString();
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "[LLM] Error:" << reply->errorString();
        qDebug() << "[LLM] HTTP:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        qDebug() << "[LLM] Body:" << QString::fromUtf8(body);
        reply->deleteLater();
        return QString();
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    qDebug() << "[LLM] Response:" << QString::fromUtf8(responseData).left(500);

    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    QJsonObject responseObj = responseDoc.object();

    QJsonArray choices = responseObj["choices"].toArray();
    if (choices.isEmpty())
    {
        qDebug() << "[LLM] No choices in response";
        return QString();
    }

    QJsonObject firstChoice = choices[0].toObject();
    QJsonObject message = firstChoice["message"].toObject();
    return message["content"].toString();
}

AnalysisCommand LLMParser::parseResponse(const QString& jsonStr) const
{
    QString json = jsonStr;

    // Extract from markdown block
    QRegularExpression re("```(?:json)?\\s*\\n?([\\s\\S]*?)```");
    auto m = re.match(jsonStr);
    if (m.hasMatch())
        json = m.captured(1).trimmed();

    // Find JSON object
    int start = json.indexOf('{');
    int end = json.lastIndexOf('}');
    if (start >= 0 && end > start)
        json = json.mid(start, end - start + 1);

    qDebug() << "[LLM] JSON:" << json;
    return AnalysisCommand::fromJsonString(json);
}
