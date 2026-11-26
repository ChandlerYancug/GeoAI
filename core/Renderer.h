#pragma once

#include <QObject>
#include <QColor>
#include <QMap>
#include <QString>
#include <memory>

class QgsVectorLayer;
class QgsRasterLayer;

// ============================================================
// Renderer — 渲染器抽象基类（策略模式）
//   OOP: 策略模式 — 不同渲染算法可互换
//   上层代码只需持有 Renderer*，不关心具体实现
// ============================================================
class Renderer : public QObject
{
    Q_OBJECT

public:
    enum RendererType { Simple, Classified, Graduated, RasterGray, RasterColorRamp };

    explicit Renderer(QObject* parent = nullptr);
    virtual ~Renderer();

    virtual void applyToVectorLayer(QgsVectorLayer* layer) = 0;
    virtual void applyToRasterLayer(QgsRasterLayer* layer) = 0;
    virtual RendererType rendererType() const = 0;

    virtual std::unique_ptr<Renderer> clone() const = 0;
};


// ============================================================
// SimpleRenderer — 简单符号渲染器
//   支持设置填充色、边框色、线宽
// ============================================================
class SimpleRenderer : public Renderer
{
    Q_OBJECT

public:
    SimpleRenderer(QObject* parent = nullptr);
    ~SimpleRenderer() override;

    void applyToVectorLayer(QgsVectorLayer* layer) override;
    void applyToRasterLayer(QgsRasterLayer* layer) override;
    RendererType rendererType() const override { return Simple; }

    std::unique_ptr<Renderer> clone() const override;

    // 符号属性
    QColor fillColor() const;
    void setFillColor(const QColor& color);
    QColor strokeColor() const;
    void setStrokeColor(const QColor& color);
    double lineWidth() const;
    void setLineWidth(double width);
    double pointSize() const;
    void setPointSize(double size);

private:
    QColor mFillColor = QColor(100, 150, 255, 180);
    QColor mStrokeColor = QColor(0, 0, 0);
    double mLineWidth = 1.0;
    double mPointSize = 4.0;
};


// ============================================================
// ClassifiedRenderer — 分类渲染器
//   按属性字段的唯一值分配不同颜色
// ============================================================
class ClassifiedRenderer : public Renderer
{
    Q_OBJECT

public:
    ClassifiedRenderer(QObject* parent = nullptr);
    ~ClassifiedRenderer() override;

    void applyToVectorLayer(QgsVectorLayer* layer) override;
    void applyToRasterLayer(QgsRasterLayer* layer) override;
    RendererType rendererType() const override { return Classified; }

    std::unique_ptr<Renderer> clone() const override;

    void setClassificationField(const QString& fieldName);
    QString classificationField() const;
    void addClassValue(const QString& value, const QColor& color);
    void autoClassify(QgsVectorLayer* layer, const QString& fieldName);
    QMap<QString, QColor> colorMap() const;

private:
    QString mFieldName;
    QMap<QString, QColor> mColorMap;

    static QColor colorFromIndex(int index);
};


// ============================================================
// GraduatedRenderer — 分级渲染器
//   按数值字段的范围分配渐变色
// ============================================================
class GraduatedRenderer : public Renderer
{
    Q_OBJECT

public:
    enum ClassificationMode { EqualInterval, Quantile, NaturalBreaks };

    GraduatedRenderer(QObject* parent = nullptr);
    ~GraduatedRenderer() override;

    void applyToVectorLayer(QgsVectorLayer* layer) override;
    void applyToRasterLayer(QgsRasterLayer* layer) override;
    RendererType rendererType() const override { return Graduated; }

    std::unique_ptr<Renderer> clone() const override;

    void setField(const QString& fieldName);
    void setClassCount(int count);
    void setColorRamp(const QColor& from, const QColor& to);
    void setMode(ClassificationMode mode);
    void autoClassify(QgsVectorLayer* layer, const QString& fieldName);

private:
    QString mFieldName;
    int mClassCount = 5;
    QColor mColorFrom = QColor(255, 255, 200);
    QColor mColorTo = QColor(200, 0, 0);
    ClassificationMode mMode = EqualInterval;
};


// ============================================================
// RasterColorRampRenderer — 栅格色带渲染
// ============================================================
class RasterColorRampRenderer : public Renderer
{
    Q_OBJECT

public:
    RasterColorRampRenderer(QObject* parent = nullptr);
    ~RasterColorRampRenderer() override;

    void applyToVectorLayer(QgsVectorLayer* layer) override;
    void applyToRasterLayer(QgsRasterLayer* layer) override;
    RendererType rendererType() const override { return RasterColorRamp; }

    std::unique_ptr<Renderer> clone() const override;

    void setColors(const QList<QColor>& colors);
    void setMinMax(double min, double max);

private:
    QList<QColor> mColors;
    double mMinValue = 0.0;
    double mMaxValue = 255.0;
};
