#pragma once

#include <QString>
#include <QList>
#include <QObject>
#include <memory>
#include <qgsrectangle.h>
#include <qgsrasterrenderer.h>

// QGIS forward declarations
class QgsMapLayer;
class QgsVectorLayer;
class QgsRasterLayer;
class QgsRectangle;

class Renderer;
class DataSource;

// ============================================================
// MapLayer — 图层抽象基类
//   OOP: 模板方法模式 — 定义 load/unload 骨架
//   OOP: 策略模式 — 持有 Renderer 指针，委托渲染
//   上层代码只需依赖此抽象接口，不关心具体图层类型
// ============================================================
class MapLayer : public QObject
{
    Q_OBJECT

public:
    enum LayerType { VectorLayerType, RasterLayerType };

    explicit MapLayer(const QString& name, QObject* parent = nullptr);
    virtual ~MapLayer();

    // —— 模板方法：加载/卸载骨架 ——
    virtual bool load() = 0;
    virtual void unload() = 0;
    virtual bool isValid() const = 0;

    // —— 公共接口 ——
    virtual QgsRectangle extent() const = 0;
    virtual LayerType layerType() const = 0;

    // 渲染器（策略模式）
    void setRenderer(std::unique_ptr<Renderer> renderer);
    Renderer* renderer() const;
    virtual void applyRenderer() = 0;

    // 数据源
    void setDataSource(std::unique_ptr<DataSource> source);
    DataSource* dataSource() const;

    // 基本属性
    QString name() const;
    void setName(const QString& name);
    QString id() const;
    bool visible() const;
    void setVisible(bool visible);
    double opacity() const { return mOpacity; }
    void setOpacity(double opacity) { mOpacity = opacity; }
    QString labelField() const { return mLabelField; }
    void setLabelField(const QString& field) { mLabelField = field; }

    // 获取底层的QGIS图层对象
    virtual QgsMapLayer* qgsMapLayer() const = 0;

signals:
    void visibilityChanged(bool visible);
    void nameChanged(const QString& name);
    void rendererChanged();

protected:
    QString mName;
    QString mId;
    bool mVisible = true;
    double mOpacity = 1.0;
    QString mLabelField;
    std::unique_ptr<Renderer> mRenderer;
    std::unique_ptr<DataSource> mDataSource;

    static int sNextId;
};


// ============================================================
// VectorLayer — 矢量图层
//   OOP: MapLayer 派生类，封装 QgsVectorLayer
// ============================================================
class VectorLayer : public MapLayer
{
    Q_OBJECT

public:
    enum GeometryType { Point, Line, Polygon, Unknown };

    explicit VectorLayer(const QString& name, QObject* parent = nullptr);
    ~VectorLayer() override;

    bool load() override;
    void unload() override;
    bool isValid() const override;

    QgsRectangle extent() const override;
    LayerType layerType() const override { return VectorLayerType; }
    void applyRenderer() override;

    QgsMapLayer* qgsMapLayer() const override;

    // 矢量特有接口
    int featureCount() const;
    GeometryType geometryType() const;
    QgsVectorLayer* qgsVectorLayer() const;

    // 从数据源加载
    void setDataSource(std::unique_ptr<DataSource> source);

private:
    QgsVectorLayer* mQgsLayer = nullptr;
};


// ============================================================
// RasterLayer — 栅格图层
//   OOP: MapLayer 派生类，封装 QgsRasterLayer
// ============================================================
class RasterLayer : public MapLayer
{
    Q_OBJECT

public:
    explicit RasterLayer(const QString& name, QObject* parent = nullptr);
    ~RasterLayer() override;

    bool load() override;
    void unload() override;
    bool isValid() const override;

    QgsRectangle extent() const override;
    LayerType layerType() const override { return RasterLayerType; }
    void applyRenderer() override;

    QgsMapLayer* qgsMapLayer() const override;

    // 栅格特有接口
    int bandCount() const;
    void setColorRamp(const QList<QColor>& colors);
    void setGrayStretch(bool enabled);
    bool grayStretch() const { return mGrayStretch; }
    QgsRasterLayer* qgsRasterLayer() const;

private:
    QgsRasterLayer* mQgsLayer = nullptr;
    bool mGrayStretch = false;
    std::unique_ptr<QgsRasterRenderer> mSavedRenderer;
};
