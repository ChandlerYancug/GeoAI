#include "MapLayer.h"
#include "DataSource.h"
#include "Renderer.h"

#include <QUuid>
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsrectangle.h>
#include <qgssinglebandgrayrenderer.h>
#include <qgscontrastenhancement.h>
#include <qgsrasterbandstats.h>

// ============================================================
// MapLayer 实现
// ============================================================

int MapLayer::sNextId = 1;

MapLayer::MapLayer(const QString& name, QObject* parent)
    : QObject(parent)
    , mName(name)
    , mId(QString("layer_%1").arg(sNextId++))
{
}

MapLayer::~MapLayer() = default;

void MapLayer::setRenderer(std::unique_ptr<Renderer> renderer)
{
    mRenderer = std::move(renderer);
}

Renderer* MapLayer::renderer() const
{
    return mRenderer.get();
}

void MapLayer::setDataSource(std::unique_ptr<DataSource> source)
{
    mDataSource = std::move(source);
}

DataSource* MapLayer::dataSource() const
{
    return mDataSource.get();
}

QString MapLayer::name() const { return mName; }
void MapLayer::setName(const QString& name) { mName = name; emit nameChanged(name); }
QString MapLayer::id() const { return mId; }
bool MapLayer::visible() const { return mVisible; }
void MapLayer::setVisible(bool visible) { mVisible = visible; emit visibilityChanged(visible); }


// ============================================================
// VectorLayer 实现
// ============================================================

VectorLayer::VectorLayer(const QString& name, QObject* parent)
    : MapLayer(name, parent)
{
}

VectorLayer::~VectorLayer()
{
    unload();
}

bool VectorLayer::load()
{
    if (mQgsLayer && mQgsLayer->isValid())
        return true;

    if (!mDataSource)
        return false;

    mDataSource->connect();
    QString uri = mDataSource->uri();
    QString provider = mDataSource->providerName();

    mQgsLayer = new QgsVectorLayer(uri, mName, provider);
    if (!mQgsLayer->isValid())
    {
        // If OGR fails, try with explicit "ogr" provider
        if (provider != "ogr")
        {
            delete mQgsLayer;
            mQgsLayer = new QgsVectorLayer(uri, mName, QStringLiteral("ogr"));
        }

        if (!mQgsLayer->isValid())
        {
            delete mQgsLayer;
            mQgsLayer = nullptr;
            return false;
        }
    }

    applyRenderer();
    return true;
}

void VectorLayer::unload()
{
    delete mQgsLayer;
    mQgsLayer = nullptr;
}

bool VectorLayer::isValid() const
{
    return mQgsLayer && mQgsLayer->isValid();
}

QgsRectangle VectorLayer::extent() const
{
    if (mQgsLayer)
        return mQgsLayer->extent();
    return QgsRectangle();
}

void VectorLayer::applyRenderer()
{
    if (mRenderer && mQgsLayer)
        mRenderer->applyToVectorLayer(mQgsLayer);
    emit rendererChanged();
}

QgsMapLayer* VectorLayer::qgsMapLayer() const
{
    return mQgsLayer;
}

int VectorLayer::featureCount() const
{
    if (mQgsLayer)
        return static_cast<int>(mQgsLayer->featureCount());
    return 0;
}

VectorLayer::GeometryType VectorLayer::geometryType() const
{
    if (!mQgsLayer) return Unknown;

    switch (mQgsLayer->geometryType())
    {
    case QgsWkbTypes::PointGeometry:   return Point;
    case QgsWkbTypes::LineGeometry:    return Line;
    case QgsWkbTypes::PolygonGeometry: return Polygon;
    default:                           return Unknown;
    }
}

QgsVectorLayer* VectorLayer::qgsVectorLayer() const
{
    return mQgsLayer;
}

void VectorLayer::setDataSource(std::unique_ptr<DataSource> source)
{
    MapLayer::setDataSource(std::move(source));
    // Keep the original layer name (don't overwrite with URI)
}


// ============================================================
// RasterLayer 实现
// ============================================================

RasterLayer::RasterLayer(const QString& name, QObject* parent)
    : MapLayer(name, parent)
{
}

RasterLayer::~RasterLayer()
{
    unload();
}

bool RasterLayer::load()
{
    if (mQgsLayer && mQgsLayer->isValid())
        return true;

    if (!mDataSource)
        return false;

    mDataSource->connect();
    QString uri = mDataSource->uri();

    mQgsLayer = new QgsRasterLayer(uri, mName);
    if (!mQgsLayer->isValid())
    {
        delete mQgsLayer;
        mQgsLayer = nullptr;
        return false;
    }

    // Let QGIS create default renderer (best quality)
    return true;
}

void RasterLayer::unload()
{
    delete mQgsLayer;
    mQgsLayer = nullptr;
}

bool RasterLayer::isValid() const
{
    return mQgsLayer && mQgsLayer->isValid();
}

QgsRectangle RasterLayer::extent() const
{
    if (mQgsLayer)
        return mQgsLayer->extent();
    return QgsRectangle();
}

void RasterLayer::applyRenderer()
{
    if (!mQgsLayer || !mQgsLayer->dataProvider()) return;

    // Save original renderer on first call
    if (!mSavedRenderer && mQgsLayer->renderer())
    {
        mSavedRenderer.reset(mQgsLayer->renderer()->clone());
    }

    if (mGrayStretch)
    {
        QgsRasterBandStats stats = mQgsLayer->dataProvider()->bandStatistics(1,
            QgsRasterBandStats::Min | QgsRasterBandStats::Max, QgsRectangle(), 250000);
        auto gray = new QgsSingleBandGrayRenderer(mQgsLayer->dataProvider(), 1);
        auto ce = new QgsContrastEnhancement(Qgis::Float32);
        ce->setMinimumValue(stats.minimumValue);
        ce->setMaximumValue(stats.maximumValue);
        ce->setContrastEnhancementAlgorithm(QgsContrastEnhancement::StretchToMinimumMaximum);
        gray->setContrastEnhancement(ce);
        mQgsLayer->setRenderer(gray);
    }
    else if (mSavedRenderer)
    {
        mQgsLayer->setRenderer(mSavedRenderer.release());
    }
    emit rendererChanged();
}

QgsMapLayer* RasterLayer::qgsMapLayer() const
{
    return mQgsLayer;
}

int RasterLayer::bandCount() const
{
    if (mQgsLayer)
        return static_cast<int>(mQgsLayer->bandCount());
    return 0;
}

void RasterLayer::setColorRamp(const QList<QColor>& colors)
{
    Q_UNUSED(colors);
}

void RasterLayer::setGrayStretch(bool enabled)
{
    mGrayStretch = enabled;
}

QgsRasterLayer* RasterLayer::qgsRasterLayer() const
{
    return mQgsLayer;
}
