#include <iostream>
#include <stdlib.h>
#include <float.h>
#include <time.h>
#include <pthread.h>
#include <sys/sysinfo.h>
#include "ray.h"

#define internal static
#define global static
#define u32Max ((u32)-1)

global u32 outputWidth = 1270, outputHeight = 720;
global u32 raycastingDepth = 32;
global u32 raysPerPixel = 8 * 4;
global u32 coreCount = 4;
global u32 tileDimension = 64; //if 0 then default.

internal u32
totalPixelSize(const Image& image)
{
    return image.width * image.height * sizeof(u32);
}

internal Image
allocateImage(const u32 width, const u32 height)
{
    Image image;
    image.width = width;
    image.height = height;

    u32 outputPixelsSize = totalPixelSize(image);
    image.pixels = (u32*)malloc(outputPixelsSize);

    return image;
}

internal void
writeImage(const Image& image, const char* filename)
{
    u32 outputPixelsSize = totalPixelSize(image);
    BitmapHeader header = {};
    header.fileType = 0x4D42;
    header.fileSize = sizeof(header) + outputPixelsSize;
    header.bitmapOffset = sizeof(header);
    header.size = sizeof(header) - 14;
    header.width = image.width;
    header.height = image.height;
    header.planes = 1;
    header.bitsPerPixel = 32;
    header.compression = 0;
    header.sizeOfBitmap = outputPixelsSize;
    header.horzResolution = 0;
    header.vertResolution = 0;
    header.colorsUsed = 0;
    header.colorsImportant = 0;

    FILE* outFile = fopen(filename, "wb");
    if (outFile)
    {
        fwrite(&header, sizeof(header), 1, outFile);
        fwrite(image.pixels, outputPixelsSize, 1, outFile);
        fclose(outFile);
    }
    else
    {
        std::cout <<"unable to write output file "<< filename << "!" << std::endl;
    }
}

internal u32
packPixel(const v3& color, const float alpha)
{
    return ((u32)color.x << 16) | ((u32)color.y << 8)
        | ((u32)color.z << 0) | ((u32)alpha << 24);
}

internal f32
rayIntersectsPlane(const v3& rayOrigin, const v3& rayDirection,
    const v3& planeNormal, const float distanceAlong)
{
    f32 epsilon = 0.00001f;
    f32 denom = dot(planeNormal, rayDirection);
    f32 distance;

    if (denom > epsilon || denom < -epsilon)
    {
        distance = (-distanceAlong - dot(planeNormal, rayOrigin))
            / denom;
    }
    else
    {
        distance = FLT_MAX;
    }

    return distance;
}

internal f32
rayIntersectsSphere(const v3& rayOrigin, const v3& rayDirection,
    const v3& spherePos, const float sphereRadius)
{
    f32 epsilon = 0.00001f;
    f32 distance = FLT_MAX;

    f32 a = dot(rayDirection, rayDirection);
    f32 b = 2.0f * dot(rayDirection, rayOrigin);
    f32 c = dot(rayOrigin, rayOrigin)
        - sphereRadius * sphereRadius;

    f32 root = sqrt(b*b - 4.0f*a*c);
    f32 denom = 2.0f * a;

    if (root > epsilon && (denom > epsilon || denom < -epsilon))
    {
        f32 t0 = (-b + root) / denom;
        f32 t1 = (-b - root) / denom;

        if (t0 < epsilon)
        {
            t0 = FLT_MAX;
        }
        if (t1 < epsilon)
        {
            t1 = FLT_MAX;
        }

        if (t0 - t1 < epsilon)
        {
            distance = t0;
        }
        else
        {
            distance = t1;
        }
    }

    return distance;
}

internal u64
lockedAddAndReturnPrev(volatile u64* value, u64 added)
{
    u64 res = __sync_fetch_and_add(value, added);
    return res;
}

u32 xorshift(randomSeries* series)
{
    u32 x = series->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    series->state = x;

    return x;
}

internal f32
randomUnilateral(randomSeries* series)
{
    return (f32)xorshift(series)/ (f32)u32Max;
}

internal f32
randomBiliteral(randomSeries* series)
{
    f32 res = -1.0f + 2.0f * randomUnilateral(series);
    return res;
}

internal f32
linearToSRGB(float l)
{
    if (l < 0.0f)
    {
        l = 0.0f;
    }
    if (l > 1.0f)
    {
        l = 1.0f;
    }

    if (l <= 0.0031308f)
    {
        return l * 12.92f;
    }
    else
    {
        return 1.055f * (f32)pow(l, 1.0f/2.4f) - 0.055f;
    }
}

internal v3
toSRGB(const v3& v)
{
    return v3(linearToSRGB(v.x), linearToSRGB(v.y), linearToSRGB(v.z)) * 255.0f;
}

internal u32*
getPixelPointer(const Image* image, const u32 x, const u32 y)
{
    u32* res = image->pixels + x + y * image->width;

    return res;
}

internal v3
rayCast(WorkQueue* queue, World* world, v3 rayOrigin,
     v3 rayDirection, randomSeries* series)
{
    f32 minHitDistance = 0.0001f;
    v3 result = v3(0, 0, 0);
    v3 attenuation = v3(1, 1, 1);
    u64 bouncesComputed = 0;

    for (u32 bounceCount = 0;
        bounceCount < raycastingDepth;
        ++bounceCount)
    {
        f32 hitDistance = FLT_MAX;
        v3 nextOrigin = {};
        v3 nextNormal = {};
        u32 hitMaterialIndex = 0;
        ++bouncesComputed;

        for (u32 planeIndex = 0;
            planeIndex < world->planesCount;
            ++planeIndex)
        {
            Plane plane = world->planes[planeIndex];

            f32 thisDistance = rayIntersectsPlane(rayOrigin,
                rayDirection, plane.normal, plane.distanceAlong);
            if (thisDistance > minHitDistance && thisDistance < hitDistance)
            {
                hitDistance = thisDistance;
                hitMaterialIndex = plane.matIndex;

                nextOrigin = rayOrigin + rayDirection * hitDistance;
                nextNormal = plane.normal;
            }
        }

        for (u32 sphereIndex = 0;
            sphereIndex < world->spheresCount;
            ++sphereIndex)
        {
            Sphere sphere = world->spheres[sphereIndex];

            v3 rayOriginRelToSphereOrigin = rayOrigin - sphere.pos;
            f32 thisDistance = rayIntersectsSphere(rayOriginRelToSphereOrigin,
                rayDirection, sphere.pos, sphere.radius);
            if (thisDistance > minHitDistance && thisDistance < hitDistance)
            {
                hitDistance = thisDistance;
                hitMaterialIndex = sphere.matIndex;

                nextOrigin = rayOrigin + rayDirection * hitDistance;
                nextNormal = normalize(nextOrigin - sphere.pos);
            }
        }

        if (hitMaterialIndex)
        {
            Material matHit = world->materials[hitMaterialIndex];
            result += hadamard(attenuation, matHit.emitColor);
            f32 cosAttenuation =
                dot(v3(0, 0, 0) - rayDirection, nextNormal);
            if (cosAttenuation < 0.0f)
            {
                cosAttenuation = 0.0f;
            }


            attenuation = hadamard(attenuation, matHit.refColor * cosAttenuation);

            rayOrigin = nextOrigin;

            v3 pureBounce = rayDirection - nextNormal
                * 2.0f*dot(rayDirection, nextNormal);

            v3 randomBounce = normalize(nextNormal
                            + v3(randomBiliteral(series), randomBiliteral(series), randomBiliteral(series)));
            rayDirection = normalize(lerp(randomBounce, pureBounce, matHit.shininess));
        }
        else
        {
            Material matHit = world->materials[hitMaterialIndex];
            result += hadamard(attenuation, matHit.emitColor);

            break;
        }
    }
    lockedAddAndReturnPrev(&queue->bouncesComputed, bouncesComputed);

    return result;
}

internal bool
renderTile(WorkQueue* queue)
{
    u64 workOrderIndex = lockedAddAndReturnPrev(&queue->nextWorkOrderIndex, 1);
    if (workOrderIndex >= queue->workOrdersCount)
    {
        return false;
    }
    WorkOrder* order = queue->workOrders + workOrderIndex;
    randomSeries series = order->series;

    World* world = order->world;
    Image image = order->image;
    u32 xMin = order->minX;
    u32 yMin = order->minY;
    u32 onePastXCount = order->onePastXCount;
    u32 onePastYCount = order->onePastYCount;

    v3 cameraPosition = v3(0, -10, 1);
    v3 cameraZ = normalize(cameraPosition);
    v3 cameraX = normalize(cross(v3(0, 0, 1), cameraZ));
    v3 cameraY = normalize(cross(cameraZ, cameraX));

    f32 filmDist = 1.0f;
    f32 filmWidth = 1.0f;
    f32 filmHeight = 1.0f;

    if (image.width > image.height)
    {
        filmHeight = (f32)image.height / (f32)image.width
            * filmWidth;
    }
    else if (image.height > image.width)
    {
        filmWidth = (f32)image.width / (f32)image.height
            * filmHeight;
    }

    f32 halfFilmWidth = 0.5f * filmWidth;
    f32 halfFilmHeight = 0.5f * filmHeight;
    v3 filmCenter = (cameraPosition - cameraZ) * filmDist;

    f32 halfPixW = 0.5f / image.width;
    f32 halfPixH = 0.5f / image.height;

    for (u32 y = yMin; y< onePastYCount; ++y)
    {
        f32 filmY = -1.0f + 2.0f*((f32)y / (f32)image.height);
        u32* out = getPixelPointer(&image, xMin, y);

        for (u32 x = xMin; x < onePastXCount; ++x)
        {
            f32 filmX = -1.0f + 2.0f*((f32)x / (f32)image.width);

            v3 color = v3(0, 0, 0);
            f32 contrib = 1.0f / (f32)raysPerPixel;
            for (u32 rayIndex = 0; rayIndex < raysPerPixel; ++rayIndex)
            {
                f32 offX = filmX + randomBiliteral(&series) * halfPixW;
                f32 offY = filmY + randomBiliteral(&series) * halfPixH;

                v3 filmPoint = filmCenter
                    + cameraX * offX * halfFilmWidth
                    + cameraY * offY * halfFilmHeight;

                v3 rayOrigin = cameraPosition;
                v3 rayDirection = normalize(filmPoint - cameraPosition);

                color += rayCast(queue, world, rayOrigin, rayDirection, &series)
                    * contrib;
            }

            color = toSRGB(color);
            f32 alpha = 1.0f;
            u32 bmpValue = packPixel(color, alpha * 255.0f);
            *out++ = bmpValue;
        }
    }

    lockedAddAndReturnPrev(&queue->tilesRetiredCount, 1);
    return true;
}

internal void*
workerThread(void* param)
{
    WorkQueue* queue = (WorkQueue*)param;
    while(renderTile(queue)) {};
    return 0;
}

internal void
createThread(void* param)
{
    pthread_t threadID;
    pthread_create(&threadID, 0, workerThread, param);
}

int main(const int argc, const char** argv)
{
    timespec startOfTheWholeProgram;
    clock_gettime(CLOCK_MONOTONIC, &startOfTheWholeProgram);

    Material materials[9] = {};
    materials[0].emitColor = v3(0.1f, 0.1f, 0.9f);
    materials[1].refColor = v3(0.1f, 0.9f, 0.1f);
    materials[2].refColor = v3(0.7f, 0.5f, 0.3f);
    materials[3].emitColor = v3(1.0f, 0.0f, 0.0f) * 8.0f;
    materials[4].refColor = v3(0.2f, 0.8f, 0.2f);
    materials[4].shininess = 0.7f;
    materials[5].refColor = v3(0.4f, 0.8f, 0.9f);
    materials[5].shininess = 0.85f;
    materials[6].refColor = v3(1.0f, 1.0f, 1.0f);
    materials[6].shininess = 1.0f;
    materials[7].emitColor = v3(1.0f, 1.0f, 1.0f) * 16.0f;
    materials[7].refColor = v3(0.0f, 0.0f, 0.0f);
    materials[7].shininess = 1.0f;
    materials[8].refColor = v3(0.0f, 0.8f, 0.3f);
    materials[8].shininess = 0.99f;
    
    Plane planes[1] = {};
    planes[0].normal = v3(0, 0, 1);
    planes[0].distanceAlong = 0;
    planes[0].matIndex = 2;

    Sphere spheres[5] = {};
    spheres[0].pos = v3(0, 0, 0);
    spheres[0].radius = 1.0f;
    spheres[0].matIndex = 2;
    spheres[1].pos = v3(3, -2, 0);
    spheres[1].radius = 1.0f;
    spheres[1].matIndex = 3;
    spheres[2].pos = v3(-2, -1, 2);
    spheres[2].radius = 1.0f;
    spheres[2].matIndex = 4;
    spheres[3].pos = v3(1, -1, 3);
    spheres[3].radius = 1.0f;
    spheres[3].matIndex = 6;
    spheres[4].pos = v3(-2, 3, 0);
    spheres[4].radius = 2.0f;
    spheres[4].matIndex = 6;
    //spheres[5].pos = v3(-4, 2, 0);
    //spheres[5].radius = 6.0f;
    //spheres[5].matIndex = 8;

    
    World world = {};
    world.materialsCount = arrayCount(materials);
    world.materials = materials;
    world.planesCount = arrayCount(planes);
    world.spheresCount = arrayCount(spheres);
    world.planes = planes;
    world.spheres = spheres;

    Image image = allocateImage(outputWidth, outputHeight);

    u32 tileWidth = image.width / coreCount;
    u32 tileHeight = tileWidth;
    if (tileDimension)
    {
        tileHeight = tileWidth = tileDimension;
    }
    u32 tileCountX = (image.width + tileWidth - 1) / tileWidth;
    u32 tileCountY = (image.height + tileHeight - 1) / tileHeight;
    u32 totalTiles = tileCountX * tileCountY;

    WorkQueue queue = {};
    queue.workOrders = (WorkOrder*)malloc(totalTiles * sizeof(WorkOrder));

    coreCount = get_nprocs();

    std::cout<<std::endl;
    std::cout<<"CONFIGURATION: " << outputWidth<<"x"<<outputHeight<<" output image size. "<<std::endl;
    std::cout<<"Raycasting depth is "<<raycastingDepth<<". "<<raysPerPixel<<" rays per one pixel."<<std::endl;
    std::cout<<coreCount<<" cores. One render tile: "<< tileWidth<< "x" <<tileHeight<<"."<<std::endl;
    std::cout<<"Tiles x: "<<tileCountX<<".Tiles y: "<<tileCountY<<". "<<std::endl;
    std::cout<<"Total number of tiles: "<< totalTiles<<". "<<std::endl;
    std::cout<<std::endl;

    timespec startOfRaycasting;
    clock_gettime(CLOCK_MONOTONIC, &startOfRaycasting);

    for (u32 tileY = 0; tileY < tileCountY; ++tileY)
    {
        u32 minY = tileY * tileHeight;
        u32 onePastMaxY = minY + tileHeight;
        if (onePastMaxY > image.height)
        {
            onePastMaxY = image.height;
        }

        for (u32 tileX = 0; tileX < tileCountX; ++tileX)
        {
            u32 minX = tileX * tileWidth;
            u32 onePastMaxX = minX + tileWidth;
            if (onePastMaxX > image.width)
            {
                onePastMaxX = image.width;
            }

            WorkOrder* order = queue.workOrders + queue.workOrdersCount++;
            order->world = &world;
            order->image = image;
            order->series.state = tileY * tileX;
            order->minX = minX;
            order->minY = minY;
            order->onePastXCount = onePastMaxX;
            order->onePastYCount = onePastMaxY;
        }
    }

    //memory fence
    lockedAddAndReturnPrev(&queue.nextWorkOrderIndex, 0);
    for(u32 coreIndex = 1; coreIndex < coreCount; ++coreIndex)
    {
        createThread(&queue);
    }

    while (queue.tilesRetiredCount < totalTiles)
    {
        if (renderTile(&queue))
        {
            std::cout<<"Raycasting progress... "<<queue.tilesRetiredCount <<"/" << totalTiles << " tiles" <<std::endl;
        }
    }

    timespec endOfRaycasting;
    clock_gettime(CLOCK_MONOTONIC, &endOfRaycasting);

    writeImage(image, "beauty.bmp");

    timespec endOfTheWholeProgram;
    clock_gettime(CLOCK_MONOTONIC, &endOfTheWholeProgram);

    f32 initTime = startOfRaycasting.tv_sec - startOfTheWholeProgram.tv_sec;
    f32 raycastingTime = endOfRaycasting.tv_sec - startOfRaycasting.tv_sec;
    f32 imageWritingTime = endOfTheWholeProgram.tv_sec - endOfRaycasting.tv_sec;
    initTime += (startOfRaycasting.tv_nsec - startOfTheWholeProgram.tv_nsec)/1000000000.0f;
    raycastingTime += (endOfRaycasting.tv_nsec - startOfRaycasting.tv_nsec)/1000000000.0f;
    imageWritingTime += (endOfTheWholeProgram.tv_nsec - endOfRaycasting.tv_nsec)/1000000000.0f;

    initTime = initTime * 1000;
    raycastingTime = raycastingTime * 1000;
    imageWritingTime = imageWritingTime * 1000;

    std::cout<<std::endl;
    std::cout<<"Init time: "<< initTime << "ms" << std::endl;
    std::cout<<"Raycasting time: "<< raycastingTime << "ms" << std::endl;
    std::cout<<"Image writing time: "<< imageWritingTime << "ms" << std::endl;
    std::cout<<std::endl;
    std::cout<<"Total bounces: "<< queue.bouncesComputed<<std::endl;
    std::cout<<"Performance: " << (f64)raycastingTime * 1000 / queue.bouncesComputed << " ms/bounce" << std::endl;
    std::cout<<std::endl;

    free(image.pixels);
    free(queue.workOrders);

    return 0;
}
