#ifndef MOTION_CONTROL_RASTERIZER2D_H
#define MOTION_CONTROL_RASTERIZER2D_H

#include <stddef.h>
#include <stdint.h>

struct RasterPoint2D {
    int32_t x;
    int32_t y;
};

struct RasterPolygon2D {
    const RasterPoint2D* vertices;
    size_t vertexCount;
};

class Rasterizer2D {
public:
    using PointCallback = void (*)(int32_t x, int32_t y, void* userData);
    using SpanCallback = void (*)(int32_t y, int32_t xStart, int32_t xEndExclusive,
        void* userData);

    // Adapted from VITRAL Rasterizer2D to a plotter-oriented context:
    // the algorithm emits pixels/spans through callbacks instead of drawing
    // into an image object.
    static void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
        PointCallback callback, void* userData = 0);
    static void drawPolygon(const RasterPolygon2D& polygon,
        PointCallback callback, void* userData = 0);
    static void fillPolygon(const RasterPolygon2D& polygon,
        SpanCallback callback, void* userData = 0);

private:
    struct FillEdge {
        int32_t yMin;
        int32_t yMaxExclusive;
        double xAtCurrentY;
        double inverseSlope;
        int32_t sortOrder;
    };

    static void sortFillEdges(FillEdge* edges, size_t count);
};

#endif
