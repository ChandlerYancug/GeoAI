#include "Renderer.h"

#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgssymbol.h>
#include <qgslinesymbollayer.h>
#include <qgscolorrampshader.h>
#include <qgsrastershader.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgssinglebandgrayrenderer.h>
#include <qgscontrastenhancement.h>
#include <qgsrasterbandstats.h>

// ============================================================
// Renderer 基类
// ============================================================

Renderer::Renderer(QObject* parent) : QObject(parent) {}
Renderer::~Renderer() = default;


// ============================================================
// SimpleRenderer
// ============================================================

SimpleRenderer::SimpleRenderer(QObject* parent) : Renderer(parent) {}
SimpleRenderer::~SimpleRenderer() = default;

void SimpleRenderer::applyToVectorLayer(QgsVectorLayer* layer)
{
    if (!layer) return;

    std::unique_ptr<QgsSymbol> symbol;
    switch (layer->geometryType())
    {
    case QgsWkbTypes::PointGeometry:
        symbol = std::make_unique<QgsMarkerSymbol>();
        break;
    case QgsWkbTypes::LineGeometry:
        symbol = std::make_unique<QgsLineSymbol>();
        break;
    case QgsWkbTypes::PolygonGeometry:
        symbol = std::make_unique<QgsFillSymbol>();
        break;
    default:
        return;
    }

    if (symbol)
    {
        symbol->setColor(mFillColor);
        if (symbol->type() == QgsSymbol::Fill || symbol->type() == QgsSymbol::Line)
        {
            // 设置边框/线条颜色和宽度
            QgsSimpleLineSymbolLayer* lineLayer = dynamic_cast<QgsSimpleLineSymbolLayer*>(
                symbol->symbolLayer(0));
            if (lineLayer)
            {
                lineLayer->setStrokeColor(mStrokeColor);
                lineLayer->setWidth(mLineWidth);
            }
        }
        if (symbol->type() == QgsSymbol::Marker)
        {
            static_cast<QgsMarkerSymbol*>(symbol.get())->setSize(mPointSize);
        }

        auto renderer = new QgsSingleSymbolRenderer(symbol.release());
        layer->setRenderer(renderer);
    }
}

void SimpleRenderer::applyToRasterLayer(QgsRasterLayer* layer)
{
    if (!layer || !layer->dataProvider()) return;

    QgsRasterBandStats stats = layer->dataProvider()->bandStatistics(1, QgsRasterBandStats::Min | QgsRasterBandStats::Max);
    double minVal = stats.minimumValue;
    double maxVal = stats.maximumValue;
    if (maxVal <= minVal) { minVal = 0; maxVal = 255; }

    auto grayRenderer = new QgsSingleBandGrayRenderer(layer->dataProvider(), 1);
    auto contrast = new QgsContrastEnhancement(Qgis::Float32);
    contrast->setMinimumValue(minVal);
    contrast->setMaximumValue(maxVal);
    contrast->setContrastEnhancementAlgorithm(QgsContrastEnhancement::StretchToMinimumMaximum);
    grayRenderer->setContrastEnhancement(contrast);
    layer->setRenderer(grayRenderer);
}

std::unique_ptr<Renderer> SimpleRenderer::clone() const
{
    auto r = std::make_unique<SimpleRenderer>();
    r->mFillColor = mFillColor;
    r->mStrokeColor = mStrokeColor;
    r->mLineWidth = mLineWidth;
    r->mPointSize = mPointSize;
    return r;
}

QColor SimpleRenderer::fillColor() const { return mFillColor; }
void SimpleRenderer::setFillColor(const QColor& color) { mFillColor = color; }
QColor SimpleRenderer::strokeColor() const { return mStrokeColor; }
void SimpleRenderer::setStrokeColor(const QColor& color) { mStrokeColor = color; }
double SimpleRenderer::lineWidth() const { return mLineWidth; }
void SimpleRenderer::setLineWidth(double width) { mLineWidth = width; }
double SimpleRenderer::pointSize() const { return mPointSize; }
void SimpleRenderer::setPointSize(double size) { mPointSize = size; }


// ============================================================
// ClassifiedRenderer
// ============================================================

ClassifiedRenderer::ClassifiedRenderer(QObject* parent) : Renderer(parent) {}
ClassifiedRenderer::~ClassifiedRenderer() = default;

void ClassifiedRenderer::applyToVectorLayer(QgsVectorLayer* layer)
{
    if (!layer || mFieldName.isEmpty()) return;

    auto renderer = new QgsCategorizedSymbolRenderer(mFieldName);

    if (!mColorMap.isEmpty())
    {
        for (auto it = mColorMap.begin(); it != mColorMap.end(); ++it)
        {
            std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(layer->geometryType()));
            symbol->setColor(it.value());
            QgsRendererCategory category(it.key(), symbol.release(), it.key());
            renderer->addCategory(category);
        }
    }

    layer->setRenderer(renderer);
}

void ClassifiedRenderer::applyToRasterLayer(QgsRasterLayer* /*layer*/) {}
std::unique_ptr<Renderer> ClassifiedRenderer::clone() const
{
    auto r = std::make_unique<ClassifiedRenderer>();
    r->mFieldName = mFieldName;
    r->mColorMap = mColorMap;
    return r;
}

void ClassifiedRenderer::setClassificationField(const QString& fieldName) { mFieldName = fieldName; }
QString ClassifiedRenderer::classificationField() const { return mFieldName; }

void ClassifiedRenderer::addClassValue(const QString& value, const QColor& color)
{
    mColorMap[value] = color;
}

void ClassifiedRenderer::autoClassify(QgsVectorLayer* layer, const QString& fieldName)
{
    if (!layer) return;
    mFieldName = fieldName;

    // 获取字段索引
    int idx = layer->fields().indexOf(fieldName);
    if (idx < 0) return;

    // 收集唯一值
    QSet<QString> uniqueValues;
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    while (it.nextFeature(f))
    {
        uniqueValues.insert(f.attribute(idx).toString());
    }

    // 分配颜色
    int colorIdx = 0;
    for (const QString& val : uniqueValues)
    {
        mColorMap[val] = colorFromIndex(colorIdx++);
    }
}

QMap<QString, QColor> ClassifiedRenderer::colorMap() const { return mColorMap; }

QColor ClassifiedRenderer::colorFromIndex(int index)
{
    static const QList<QColor> palette = {
        QColor(230, 25, 75),    QColor(60, 180, 75),   QColor(255, 225, 25),
        QColor(0, 130, 200),    QColor(245, 130, 48),  QColor(145, 30, 180),
        QColor(70, 240, 240),   QColor(240, 50, 230),  QColor(210, 245, 60),
        QColor(250, 190, 212),  QColor(0, 128, 128),   QColor(220, 190, 255),
        QColor(170, 110, 40),   QColor(255, 250, 200), QColor(128, 0, 0),
        QColor(170, 255, 195),  QColor(128, 128, 0),   QColor(255, 215, 180),
        QColor(0, 0, 128),      QColor(128, 128, 128),
    };
    return palette[index % palette.size()];
}


// ============================================================
// GraduatedRenderer
// ============================================================

GraduatedRenderer::GraduatedRenderer(QObject* parent) : Renderer(parent) {}
GraduatedRenderer::~GraduatedRenderer() = default;

void GraduatedRenderer::applyToVectorLayer(QgsVectorLayer* layer)
{
    if (!layer || mFieldName.isEmpty()) return;

    auto renderer = new QgsGraduatedSymbolRenderer(mFieldName);

    // 获取字段值范围
    double minVal = 0.0, maxVal = 1.0;
    QgsFeatureIterator it = layer->getFeatures();
    QgsFeature f;
    int fieldIdx = layer->fields().indexOf(mFieldName);
    if (fieldIdx >= 0)
    {
        bool first = true;
        while (it.nextFeature(f))
        {
            double val = f.attribute(fieldIdx).toDouble();
            if (first) { minVal = maxVal = val; first = false; }
            else { minVal = qMin(minVal, val); maxVal = qMax(maxVal, val); }
        }
    }

    // 创建分级范围
    double range = (maxVal - minVal) / mClassCount;
    for (int i = 0; i < mClassCount; ++i)
    {
        double lower = minVal + i * range;
        double upper = (i == mClassCount - 1) ? maxVal : minVal + (i + 1) * range;

        // 插值颜色
        double ratio = static_cast<double>(i) / qMax(1, mClassCount - 1);
        int r = static_cast<int>(mColorFrom.red()   + ratio * (mColorTo.red()   - mColorFrom.red()));
        int g = static_cast<int>(mColorFrom.green() + ratio * (mColorTo.green() - mColorFrom.green()));
        int b = static_cast<int>(mColorFrom.blue()  + ratio * (mColorTo.blue()  - mColorFrom.blue()));
        QColor color(r, g, b, 180);

        std::unique_ptr<QgsSymbol> symbol(QgsSymbol::defaultSymbol(layer->geometryType()));
        symbol->setColor(color);

        QgsRendererRange rangeObj(lower, upper, symbol.release(),
                                  QString("%1 - %2").arg(lower, 0, 'f', 1).arg(upper, 0, 'f', 1));
        renderer->addClass(rangeObj);
    }

    layer->setRenderer(renderer);
}

void GraduatedRenderer::applyToRasterLayer(QgsRasterLayer* /*layer*/) {}
std::unique_ptr<Renderer> GraduatedRenderer::clone() const
{
    auto r = std::make_unique<GraduatedRenderer>();
    r->mFieldName = mFieldName;
    r->mClassCount = mClassCount;
    r->mColorFrom = mColorFrom;
    r->mColorTo = mColorTo;
    r->mMode = mMode;
    return r;
}

void GraduatedRenderer::setField(const QString& fieldName) { mFieldName = fieldName; }
void GraduatedRenderer::setClassCount(int count) { mClassCount = count; }
void GraduatedRenderer::setColorRamp(const QColor& from, const QColor& to) { mColorFrom = from; mColorTo = to; }
void GraduatedRenderer::setMode(ClassificationMode mode) { mMode = mode; }
void GraduatedRenderer::autoClassify(QgsVectorLayer* layer, const QString& fieldName)
{
    if (!layer) return;
    setField(fieldName);
    applyToVectorLayer(layer);
}


// ============================================================
// RasterColorRampRenderer
// ============================================================

RasterColorRampRenderer::RasterColorRampRenderer(QObject* parent) : Renderer(parent)
{
    // 默认色带：蓝-绿-黄-红
    mColors = {
        QColor(0, 0, 255),
        QColor(0, 255, 0),
        QColor(255, 255, 0),
        QColor(255, 0, 0)
    };
}

RasterColorRampRenderer::~RasterColorRampRenderer() = default;

void RasterColorRampRenderer::applyToVectorLayer(QgsVectorLayer* /*layer*/) {}

void RasterColorRampRenderer::applyToRasterLayer(QgsRasterLayer* layer)
{
    if (!layer || !layer->dataProvider()) return;

    QgsRasterBandStats stats = layer->dataProvider()->bandStatistics(1,
        QgsRasterBandStats::Min | QgsRasterBandStats::Max | QgsRasterBandStats::Mean, QgsRectangle(), 250000);
    double minVal = stats.minimumValue;
    double maxVal = stats.maximumValue;
    if (maxVal <= minVal) { minVal = 0; maxVal = 255; }

    // Stretch range slightly for better visual contrast (skip top/bottom 2%)
    double stretch = (maxVal - minVal) * 0.02;
    minVal = minVal + stretch;
    maxVal = maxVal - stretch;

    auto shader = new QgsColorRampShader(minVal, maxVal);

    QList<QgsColorRampShader::ColorRampItem> items;
    int n = mColors.size();
    double step = (maxVal - minVal) / (n - 1);
    for (int i = 0; i < n; ++i)
    {
        double val = minVal + i * step;
        items.append(QgsColorRampShader::ColorRampItem(val, mColors[i],
                        QString::number(val, 'f', 1)));
    }
    shader->setColorRampItemList(items);
    shader->setColorRampType(QgsColorRampShader::Interpolated);

    auto rasterShader = new QgsRasterShader();
    rasterShader->setRasterShaderFunction(shader);

    auto renderer = new QgsSingleBandPseudoColorRenderer(layer->dataProvider(), 1, rasterShader);
    layer->setRenderer(renderer);
}

std::unique_ptr<Renderer> RasterColorRampRenderer::clone() const
{
    auto r = std::make_unique<RasterColorRampRenderer>();
    r->mColors = mColors;
    r->mMinValue = mMinValue;
    r->mMaxValue = mMaxValue;
    return r;
}

void RasterColorRampRenderer::setColors(const QList<QColor>& colors) { mColors = colors; }
void RasterColorRampRenderer::setMinMax(double min, double max) { mMinValue = min; mMaxValue = max; }
