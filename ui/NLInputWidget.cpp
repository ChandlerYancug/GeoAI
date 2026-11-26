#include "NLInputWidget.h"
#include "../nlp/QueryParser.h"
#include "../nlp/RuleBasedParser.h"
#include "../nlp/LLMParser.h"
#include "../core/AnalysisCommand.h"

#include <QHBoxLayout>
#include <QApplication>
#include <QDateTime>
#include <QSettings>
#include <QFont>

NLInputWidget::NLInputWidget(QWidget* parent) : QWidget(parent)
{
    setupUI();

    // 默认使用规则解析器
    mParser = std::make_unique<RuleBasedParser>();
}

NLInputWidget::~NLInputWidget() = default;

void NLInputWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 标题
    auto* titleLabel = new QLabel("自然语言查询", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; padding: 4px;");
    mainLayout->addWidget(titleLabel);

    // 说明
    auto* hintLabel = new QLabel("支持: 找距离/查询属性/空间关系/叠加分析", this);
    hintLabel->setStyleSheet("color: gray; font-size: 11px;");
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    // 输入行
    auto* inputLayout = new QHBoxLayout();
    mInputEdit = new QLineEdit(this);
    mInputEdit->setPlaceholderText("例: 找出距离河流200米内的建筑");
    mInputEdit->setMinimumHeight(32);

    mSubmitButton = new QPushButton("查询", this);
    mSubmitButton->setMinimumHeight(32);
    mSubmitButton->setStyleSheet(
        "QPushButton { background-color: #2196F3; color: white; padding: 4px 16px; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #0b7dda; }");

    inputLayout->addWidget(mInputEdit, 1);
    inputLayout->addWidget(mSubmitButton);
    mainLayout->addLayout(inputLayout);

    // AI开关和API Key
    auto* aiLayout = new QHBoxLayout();
    mAiCheck = new QCheckBox("AI增强", this);
    mAiCheck->setToolTip("启用大模型 (DeepSeek) 增强NL解析");
    mAiKeyEdit = new QLineEdit(this);
    mAiKeyEdit->setPlaceholderText("API Key (自动保存)");
    mAiKeyEdit->setEchoMode(QLineEdit::Password);

    // Load saved key (deferred to avoid crash during init)
    QSettings settings("GeoAI", "GeoAI");
    QString savedKey = settings.value("api_key").toString();
    if (!savedKey.isEmpty()) {
        mAiKeyEdit->setText(savedKey);
    }
    mAiKeyEdit->setVisible(false);
    connect(mAiCheck, &QCheckBox::toggled, this, [this](bool on) {
        mAiKeyEdit->setVisible(on);
        setLLMMode(on, mAiKeyEdit->text());
    });
    connect(mAiKeyEdit, &QLineEdit::textChanged, this, [this](const QString& key) {
        if (mAiCheck->isChecked()) setLLMMode(true, key);
        QSettings settings("GeoAI", "GeoAI");
        settings.setValue("api_key", key);
    });
    aiLayout->addWidget(mAiCheck);
    aiLayout->addWidget(mAiKeyEdit, 1);
    mainLayout->addLayout(aiLayout);

    // 状态标签
    mStatusLabel = new QLabel("就绪", this);
    mStatusLabel->setStyleSheet("color: green; font-size: 11px;");
    mainLayout->addWidget(mStatusLabel);

    // 结果显示
    auto* resultLabel = new QLabel("分析结果:", this);
    resultLabel->setStyleSheet("font-weight: bold; margin-top: 8px;");
    mainLayout->addWidget(resultLabel);

    mResultDisplay = new QTextEdit(this);
    mResultDisplay->setReadOnly(true);
    mResultDisplay->setMaximumHeight(120);
    mResultDisplay->setPlaceholderText("解析结果将显示在这里...");
    mainLayout->addWidget(mResultDisplay);

    // 历史记录
    auto* historyLabel = new QLabel("查询历史:", this);
    historyLabel->setStyleSheet("font-weight: bold; margin-top: 8px;");
    mainLayout->addWidget(historyLabel);

    mHistoryDisplay = new QTextEdit(this);
    mHistoryDisplay->setReadOnly(true);
    mHistoryDisplay->setPlaceholderText("查询历史记录...");
    mainLayout->addWidget(mHistoryDisplay, 1);

    // 连接信号
    connect(mSubmitButton, &QPushButton::clicked,
            this, &NLInputWidget::onSubmitClicked);
    connect(mInputEdit, &QLineEdit::returnPressed,
            this, &NLInputWidget::onInputReturnPressed);
}

void NLInputWidget::setParser(std::unique_ptr<QueryParser> parser)
{
    mParser = std::move(parser);
}

QueryParser* NLInputWidget::parser() const
{
    return mParser.get();
}

void NLInputWidget::setLLMMode(bool enabled, const QString& apiKey)
{
    if (enabled && !apiKey.isEmpty())
    {
        auto llm = std::make_unique<LLMParser>();
        llm->setApiKey(apiKey);
        mParser = std::move(llm);
        mStatusLabel->setText("AI模式已启用");
        mStatusLabel->setStyleSheet("color: blue; font-size: 11px;");
    }
    else
    {
        mParser = std::make_unique<RuleBasedParser>();
        mStatusLabel->setText("规则解析模式");
        mStatusLabel->setStyleSheet("color: green; font-size: 11px;");
    }
}

void NLInputWidget::onSubmitClicked()
{
    QString text = mInputEdit->text().trimmed();
    if (text.isEmpty()) return;

    // 先发出原始文本信号
    emit queryTextSubmitted(text);

    // 使用解析器解析
    if (mParser)
    {
        // Show thinking indicator for AI mode
        if (dynamic_cast<LLMParser*>(mParser.get())) {
            mResultDisplay->setText("🤔 AI思考中...");
            QApplication::processEvents();
        }

        AnalysisCommand cmd = mParser->parse(text);

        if (cmd.isValid())
        {
            mStatusLabel->setText("✓ 解析成功");
            mStatusLabel->setStyleSheet("color: green; font-size: 11px;");

            // Show result
            auto* llm = dynamic_cast<LLMParser*>(mParser.get());
            if (llm && !llm->lastReply().isEmpty()) {
                mResultDisplay->setText(llm->lastReply());
            } else {
                QJsonObject json = cmd.toJson();
                QJsonDocument doc(json);
                mResultDisplay->setText(doc.toJson(QJsonDocument::Indented));
            }

            // 添加到历史
            addHistoryEntry(text, cmd);

            // 发出解析结果
            emit querySubmitted(cmd);
        }
        else
        {
            bool isAI = dynamic_cast<LLMParser*>(mParser.get()) != nullptr;
            mStatusLabel->setText(isAI ? "AI解析失败,已回退规则解析" : "无法解析查询");
            mStatusLabel->setStyleSheet("color: red; font-size: 11px;");
            mResultDisplay->setText("解析失败。请尝试:\n- 找出距离XX米内的XX\n- 查询XX大于XX的XX\n- 显示与XX重叠的XX");
        }
    }
    else
    {
        mStatusLabel->setText("⚠ 未配置解析器");
        mStatusLabel->setStyleSheet("color: orange; font-size: 11px;");
    }
}

void NLInputWidget::onInputReturnPressed()
{
    onSubmitClicked();
}

void NLInputWidget::addHistoryEntry(const QString& query, const AnalysisCommand& result)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString entry = QString("[%1] %2\n  → %3\n")
        .arg(timestamp, query, result.description());

    // 插入到历史开头
    QString current = mHistoryDisplay->toPlainText();
    mHistoryDisplay->setText(entry + "\n" + current);
}
