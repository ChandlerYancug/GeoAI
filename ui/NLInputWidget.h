#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <memory>

class QueryParser;
class AnalysisCommand;

// ============================================================
// NLInputWidget — 自然语言输入控件
//   OOP: 单一职责 — 只负责NL输入和结果显示
//   解析由 QueryParser 完成
// ============================================================
class NLInputWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NLInputWidget(QWidget* parent = nullptr);
    ~NLInputWidget() override;

    // 设置解析器
    void setParser(std::unique_ptr<QueryParser> parser);
    QueryParser* parser() const;
    void setLLMMode(bool enabled, const QString& apiKey = QString());

    // 添加历史记录
    void addHistoryEntry(const QString& query, const AnalysisCommand& result);

signals:
    void querySubmitted(const AnalysisCommand& command);
    void queryTextSubmitted(const QString& text);

private slots:
    void onSubmitClicked();
    void onInputReturnPressed();

private:
    void setupUI();

    std::unique_ptr<QueryParser> mParser;
    QLineEdit* mInputEdit;
    QPushButton* mSubmitButton;
    QTextEdit* mResultDisplay;
    QTextEdit* mHistoryDisplay;
    QLabel* mStatusLabel;
    QCheckBox* mAiCheck;
    QLineEdit* mAiKeyEdit;
};
