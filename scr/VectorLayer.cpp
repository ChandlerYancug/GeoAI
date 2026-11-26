/* FileName: VectorLayer.cpp
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 矢量图层实现 - 封装 QgsVectorLayer
*/

#include "VectorLayer.h"
#include <qgsvectorlayer.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>

class VectorLayerImpl : public VectorLayer {
private:
    QgsVectorLayer* m_pLayer;
    QString m_sName;
    bool m_bVisible;

public:
    VectorLayerImpl(const QString& sPath, const QString& sName)
        : m_bVisible(true)
        , m_sName(sName)
    {
        m_pLayer = new QgsVectorLayer(sPath, sName, "ogr");
    }

    ~VectorLayerImpl() override {
        delete m_pLayer;
    }

    QString getLayerName() const override {
        return m_sName;
    }

    void setVisible(bool bVisible) override {
        m_bVisible = bVisible;
        // 注：当前QGIS版本的QgsVectorLayer没有setVisible方法
        // 可见性由图层管理器统一控制
    }

    bool isVisible() const override {
        return m_bVisible;
    }

    QRectF getExtent() const override {
        if (!m_pLayer) return QRectF();
        QgsRectangle oRect = m_pLayer->extent();
        return QRectF(oRect.xMinimum(), oRect.yMinimum(),
            oRect.width(), oRect.height());
    }

    void* getInternalLayer() const override {
        return static_cast<void*>(m_pLayer);
    }

    int getFeatureCount() const override {
        if (!m_pLayer) return 0;
        return m_pLayer->featureCount();
    }

    std::vector<Geometry> getFeatures() const override {
        std::vector<Geometry> vGeometries;
        if (!m_pLayer) return vGeometries;

        QgsFeatureIterator it = m_pLayer->getFeatures();
        QgsFeature oFeature;
        while (it.nextFeature(oFeature)) {
            QgsGeometry oGeom = oFeature.geometry();
            if (oGeom.isNull()) continue;

            /* 这里需要将 QgsGeometry 转换为您的 Geometry 类型 */
            /* 暂时创建空几何，后续完善 */
            vGeometries.push_back(Geometry(Point(0, 0)));
        }
        return vGeometries;
    }

    Geometry getFeatureAt(int nIndex) const override {
        if (!m_pLayer || nIndex < 0) return Geometry(Point(0, 0));
        /* 简化实现，返回空点 */
        return Geometry(Point(0, 0));
    }

    bool importFromDatabase(const QString& sTableName) override {
        /* 暂未实现，等待模块二队友定义格式 */
        return false;
    }

    bool exportToDatabase(const QString& sTableName) override {
        /* 暂未实现，等待模块二队友定义格式 */
        return false;
    }
};

/* 工厂函数：创建矢量图层 */
VectorLayer* VectorLayer::createFromFile(const QString& sPath, const QString& sName) {
    return new VectorLayerImpl(sPath, sName);
}