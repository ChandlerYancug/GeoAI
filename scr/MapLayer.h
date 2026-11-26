/* FileName: MapLayer.h
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 图层抽象基类 - 供模块二/三/四/五使用
*/

#ifndef MAPLAYER_H
#define MAPLAYER_H

#include <QString>
#include <QRectF>

class MapLayer {
public:
    virtual ~MapLayer() = default;

    /* 获取图层名称（模块四用） */
    virtual QString getLayerName() const = 0;

    /* 设置/获取可见性 */
    virtual void setVisible(bool bVisible) = 0;
    virtual bool isVisible() const = 0;

    /* 获取图层范围（模块三用） */
    virtual QRectF getExtent() const = 0;

    /* 获取内部QGIS图层指针（模块五用） */
    virtual void* getInternalLayer() const = 0;
};

#endif