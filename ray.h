#include <stdint.h>
#include "math.h"

#define arrayCount(array) (sizeof(array) / sizeof(array[0]))

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

#pragma pack(push, 1)
struct BitmapHeader
{
    u16 fileType;
    u32 fileSize;
    u16 reserved1;
    u16 reserved2;
    u32 bitmapOffset;
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bitsPerPixel;
    u32 compression;
    u32 sizeOfBitmap;
    s32 horzResolution;
    s32 vertResolution;
    u32 colorsUsed;
    u32 colorsImportant;
};
#pragma pack(pop)

struct Image
{
    u32 width;
    u32 height;
    u32* pixels;
};

struct Material
{
    f32 shininess;
    v3 emitColor;
    v3 refColor;
};

struct Plane
{
    v3 normal;
    f32 distanceAlong;
    u32 matIndex;
};

struct Sphere
{
    v3 pos;
    f32 radius;
    u32 matIndex;
};

struct World
{
    u32 materialsCount;
    Material* materials;

    u32 planesCount;
    Plane* planes;

    u32 spheresCount;
    Sphere* spheres;
};

struct randomSeries
{
    u32 state;
};

struct WorkOrder
{
    World* world;
    Image image;
    randomSeries series;
    u32 minX;
    u32 minY;
    u32 onePastXCount;
    u32 onePastYCount;
};

struct WorkQueue
{
    u32 workOrdersCount;
    WorkOrder* workOrders;

    volatile u64 bouncesComputed;
    volatile u64 tilesRetiredCount;
    volatile u64 nextWorkOrderIndex;
};
