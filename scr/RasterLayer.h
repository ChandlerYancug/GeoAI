/* FileName: RasterLayer.h
   Author: YourName
   Version: 1.0
   Date: 2026-05-25
   Description: 栅格图层接口 - 供模块三使用
*/

#ifndef RASTERLAYER_H
#define RASTERLAYER_H

#include "MapLayer.h"

class RasterLayer : public MapLayer {
public:
    virtual ~RasterLayer() = default;

    /* 获取波段数量 */
    virtual int getBandCount() const = 0;

    /* 获取像素值 */
    virtual double getPixelValue(int nCol, int nRow, int nBand = 1) const = 0;

    /* 获取尺寸 */
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};

#endif