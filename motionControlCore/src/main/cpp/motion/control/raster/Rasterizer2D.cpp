#include "motion/control/raster/Rasterizer2D.h"

#include <math.h>
#include <stdlib.h>

void Rasterizer2D::sortFillEdges(FillEdge* edges, size_t count)
{
    for (size_t i = 1; i < count; ++i) {
        FillEdge key = edges[i];
        size_t j = i;
        while (j > 0) {
            const FillEdge& prev = edges[j - 1];
            bool shouldMove = false;

            if (prev.xAtCurrentY > key.xAtCurrentY) {
                shouldMove = true;
            }
            else if (prev.xAtCurrentY == key.xAtCurrentY &&
                prev.inverseSlope > key.inverseSlope) {
                shouldMove = true;
            }
            else if (prev.xAtCurrentY == key.xAtCurrentY &&
                prev.inverseSlope == key.inverseSlope &&
                prev.sortOrder > key.sortOrder) {
                shouldMove = true;
            }

            if (!shouldMove) {
                break;
            }

            edges[j] = prev;
            --j;
        }
        edges[j] = key;
    }
}

void Rasterizer2D::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
    PointCallback callback, void* userData)
{
    if (!callback) {
        return;
    }

    int32_t dx = static_cast<int32_t>(abs(x1 - x0));
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = -static_cast<int32_t>(abs(y1 - y0));
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;

    while (true) {
        callback(x0, y0, userData);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Rasterizer2D::drawPolygon(const RasterPolygon2D& polygon,
    PointCallback callback, void* userData)
{
    if (!polygon.vertices || polygon.vertexCount < 2 || !callback) {
        return;
    }

    for (size_t i = 0; i < polygon.vertexCount; ++i) {
        const RasterPoint2D& a = polygon.vertices[i];
        const RasterPoint2D& b = polygon.vertices[(i + 1) % polygon.vertexCount];
        drawLine(a.x, a.y, b.x, b.y, callback, userData);
    }
}

void Rasterizer2D::fillPolygon(const RasterPolygon2D& polygon,
    SpanCallback callback, void* userData)
{
    if (!polygon.vertices || polygon.vertexCount < 3 || !callback) {
        return;
    }

    int32_t minY = polygon.vertices[0].y;
    int32_t maxYExclusive = polygon.vertices[0].y + 1;
    for (size_t i = 0; i < polygon.vertexCount; ++i) {
        const RasterPoint2D& point = polygon.vertices[i];
        if (point.y < minY) {
            minY = point.y;
        }
        if ((point.y + 1) > maxYExclusive) {
            maxYExclusive = point.y + 1;
        }
    }

    const size_t bucketCount = (maxYExclusive > minY) ?
        static_cast<size_t>(maxYExclusive - minY) : 0;
    if (bucketCount == 0) {
        return;
    }

    size_t* bucketSizes = static_cast<size_t*>(malloc(sizeof(size_t) * bucketCount));
    size_t* bucketOffsets = static_cast<size_t*>(malloc(sizeof(size_t) * bucketCount));
    FillEdge* bucketStorage =
        static_cast<FillEdge*>(malloc(sizeof(FillEdge) * polygon.vertexCount));
    FillEdge* activeEdges =
        static_cast<FillEdge*>(malloc(sizeof(FillEdge) * polygon.vertexCount));

    if (!bucketSizes || !bucketOffsets || !bucketStorage || !activeEdges) {
        free(bucketSizes);
        free(bucketOffsets);
        free(bucketStorage);
        free(activeEdges);
        return;
    }

    for (size_t i = 0; i < bucketCount; ++i) {
        bucketSizes[i] = 0;
        bucketOffsets[i] = 0;
    }

    int32_t sortOrder = 0;
    for (size_t i = 0; i < polygon.vertexCount; ++i) {
        const RasterPoint2D& a = polygon.vertices[i];
        const RasterPoint2D& b = polygon.vertices[(i + 1) % polygon.vertexCount];

        if (a.y == b.y) {
            continue;
        }

        const RasterPoint2D* top = &a;
        const RasterPoint2D* bottom = &b;
        if (a.y > b.y) {
            top = &b;
            bottom = &a;
        }

        const int32_t yMin = top->y;
        const int32_t yMax = bottom->y;
        if (yMin >= yMax) {
            continue;
        }

        ++bucketSizes[static_cast<size_t>(yMin - minY)];
    }

    size_t runningOffset = 0;
    for (size_t i = 0; i < bucketCount; ++i) {
        bucketOffsets[i] = runningOffset;
        runningOffset += bucketSizes[i];
        bucketSizes[i] = 0;
    }

    for (size_t i = 0; i < polygon.vertexCount; ++i) {
        const RasterPoint2D& a = polygon.vertices[i];
        const RasterPoint2D& b = polygon.vertices[(i + 1) % polygon.vertexCount];

        if (a.y == b.y) {
            continue;
        }

        const RasterPoint2D* top = &a;
        const RasterPoint2D* bottom = &b;
        if (a.y > b.y) {
            top = &b;
            bottom = &a;
        }

        const int32_t yMin = top->y;
        const int32_t yMax = bottom->y;
        if (yMin >= yMax) {
            continue;
        }

        const double inverseSlope =
            static_cast<double>(bottom->x - top->x) /
            static_cast<double>(bottom->y - top->y);

        FillEdge edge;
        edge.yMin = yMin;
        edge.yMaxExclusive = yMax;
        edge.xAtCurrentY = static_cast<double>(top->x);
        edge.inverseSlope = inverseSlope;
        edge.sortOrder = sortOrder++;

        const size_t bucketIndex = static_cast<size_t>(yMin - minY);
        const size_t writeIndex = bucketOffsets[bucketIndex] + bucketSizes[bucketIndex];
        bucketStorage[writeIndex] = edge;
        ++bucketSizes[bucketIndex];
    }

    size_t activeCount = 0;
    for (int32_t y = minY; y < maxYExclusive; ++y) {
        const size_t bucketIndex = static_cast<size_t>(y - minY);
        const size_t offset = bucketOffsets[bucketIndex];
        const size_t count = bucketSizes[bucketIndex];

        for (size_t i = 0; i < count; ++i) {
            activeEdges[activeCount++] = bucketStorage[offset + i];
        }

        size_t writePos = 0;
        for (size_t i = 0; i < activeCount; ++i) {
            if (y < activeEdges[i].yMaxExclusive) {
                activeEdges[writePos++] = activeEdges[i];
            }
        }
        activeCount = writePos;

        if (activeCount < 2) {
            for (size_t i = 0; i < activeCount; ++i) {
                activeEdges[i].xAtCurrentY += activeEdges[i].inverseSlope;
            }
            continue;
        }

        sortFillEdges(activeEdges, activeCount);

        for (size_t i = 0; i + 1 < activeCount; i += 2) {
            double xLeft = activeEdges[i].xAtCurrentY;
            double xRight = activeEdges[i + 1].xAtCurrentY;

            if (xLeft > xRight) {
                const double temp = xLeft;
                xLeft = xRight;
                xRight = temp;
            }

            const int32_t xStart = static_cast<int32_t>(ceil(xLeft));
            const int32_t xEndExclusive = static_cast<int32_t>(ceil(xRight));
            if (xStart < xEndExclusive) {
                callback(y, xStart, xEndExclusive, userData);
            }
        }

        for (size_t i = 0; i < activeCount; ++i) {
            activeEdges[i].xAtCurrentY += activeEdges[i].inverseSlope;
        }
    }

    free(bucketSizes);
    free(bucketOffsets);
    free(bucketStorage);
    free(activeEdges);
}
