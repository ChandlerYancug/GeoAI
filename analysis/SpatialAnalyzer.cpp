#include "SpatialAnalyzer.h"

// ============================================================
// SpatialAnalyzer 基类实现
// ============================================================

SpatialAnalyzer::SpatialAnalyzer(QObject* parent) : QObject(parent) {}
SpatialAnalyzer::~SpatialAnalyzer() = default;

void SpatialAnalyzer::setParameters(std::unique_ptr<AnalysisParam> params)
{
    mParams = std::move(params);
}

AnalysisParam* SpatialAnalyzer::parameters() const
{
    return mParams.get();
}

void SpatialAnalyzer::setInputLayers(const QList<MapLayer*>& layers)
{
    mInputLayers = layers;
}

QList<MapLayer*> SpatialAnalyzer::inputLayers() const
{
    return mInputLayers;
}
