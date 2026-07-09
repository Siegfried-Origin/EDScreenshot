#include "FrameExport.h"

#include <algorithm>

#define _USE_MATH_DEFINES 
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#undef min
#undef max
#include <tinyexr.h>
#include <tiffio.h>


float FrameExport::DecodeR11G11B10Component(
    uint32_t mantissa,
    uint32_t exponent,
    uint32_t mantissaBits)
{
    if (exponent == 0) {
        if (mantissa == 0) {
            return 0.0f;
        }

        // denormal
        return ldexpf(
            (float)mantissa / (float)(1u << mantissaBits),
            -14);
    }

    if (exponent == 31) {
        return INFINITY;
    }

    const float m = 1.0f +
        ((float)mantissa / (float)(1u << mantissaBits));

    return ldexpf(m, (int)exponent - 15);
}


void FrameExport::DecodeR11G11B10Float(
    uint32_t packed,
    float& r,
    float& g,
    float& b)
{
    const uint32_t rm = packed & 0x3F;
    const uint32_t re = (packed >> 6) & 0x1F;

    const uint32_t gm = (packed >> 11) & 0x3F;
    const uint32_t ge = (packed >> 17) & 0x1F;

    const uint32_t bm = (packed >> 22) & 0x1F;
    const uint32_t be = (packed >> 27) & 0x1F;

    r = DecodeR11G11B10Component(rm, re, 6);
    g = DecodeR11G11B10Component(gm, ge, 6);
    b = DecodeR11G11B10Component(bm, be, 5);
}


void FrameExport::GetTextureSize(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* srcTex,
    uint32_t* width, uint32_t* height)
{
    D3D11_TEXTURE2D_DESC desc;
    srcTex->GetDesc(&desc);

    *width = desc.Width;
    *height = desc.Height;
}


bool FrameExport::GetTexture(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* srcTex,
    std::vector<float>& textureData,
    uint32_t& width, uint32_t& height)
{
    D3D11_TEXTURE2D_DESC desc;
    srcTex->GetDesc(&desc);

    if (desc.Format != DXGI_FORMAT_R11G11B10_FLOAT) {
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;

    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);

    if (FAILED(hr)) {
        return false;
    }

    context->CopyResource(staging, srcTex);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);

    if (FAILED(hr)) {
        staging->Release();
        return false;
    }

    width = desc.Width;
    height = desc.Height;

    if (textureData.size() < 3 * width * height) {
        textureData.resize(3 * width * height);
    }

    for (int y = 0; y < height; y++) {
        const uint32_t* row =
            (const uint32_t*)
            ((const uint8_t*)mapped.pData +
                y * mapped.RowPitch);

        for (int x = 0; x < width; x++) {
            float r, g, b;
            DecodeR11G11B10Float(row[x], r, g, b);

            const int idx = 3 * (y * width + x);
            textureData[idx + 0] = r;
            textureData[idx + 1] = g;
            textureData[idx + 2] = b;
        }
    }

    context->Unmap(staging, 0);
    staging->Release();
}


bool FrameExport::AppendTextureTile(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* srcTex,
    std::vector<float>& textureData,
    uint32_t width, uint32_t height,
    int startX, int startY)
{
    D3D11_TEXTURE2D_DESC desc;
    srcTex->GetDesc(&desc);

    if (desc.Format != DXGI_FORMAT_R11G11B10_FLOAT) {
        return false;
    }

    // --- CLIPPING LOGIC FOR ALL 4 SIDES ---

    // 1. Find the intersection in Image Space
    // The leftmost pixel we can copy is either 0 or startX, whichever is further right
    int intersectLeft = std::max(0, startX);
    // The rightmost pixel is either image width or the end of the tile, whichever is further left
    int intersectRight = std::min((int)width, (int)(startX + desc.Width));

    int intersectTop = std::max(0, startY);
    int intersectBottom = std::min((int)height, (int)(startY + desc.Height));

    // If there is no overlap, just return true (nothing to do)
    if (intersectLeft >= intersectRight || intersectTop >= intersectBottom) {
        return true;
    }

    // 2. Map Image Space intersection back to Tile Space (Source Texture coordinates)
    // Example: if startX is -10, and intersectLeft is 0, we must start reading at tile pixel 10.
    int srcXStart = intersectLeft - startX;
    int srcYStart = intersectTop - startY;
    int srcXEnd   = intersectRight - startX;
    int srcYEnd   = intersectBottom - startY;

    // --- CLIPPING LOGIC END ---

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);

    if (FAILED(hr)) {
        return false;
    }

    context->CopyResource(staging, srcTex);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);

    if (FAILED(hr)) {
        staging->Release();
        return false;
    }

    const float featherMargin = std::min(desc.Width, desc.Height) / 2.f;

    // Loop through the source texture using our calculated tile-space boundaries
    for (int y = srcYStart; y < srcYEnd; y++) {
        const uint32_t* row =
            (const uint32_t*)
            ((const uint8_t*)mapped.pData +
                y * mapped.RowPitch);

        for (int x = srcXStart; x < srcXEnd; x++) {
            float r, g, b;
            DecodeR11G11B10Float(row[x], r, g, b);

            // Calculate destination index using the same relative offset
            // Destination X = startX + source X
            const int destX = startX + x;
            const int destY = startY + y;
            const int idx = 4 * (destY * width + destX);

            float distToEdge = std::min({
                (float)x,
                (float)(desc.Width - 1 - x),
                (float)y,
                (float)(desc.Height - 1 - y)
            });

            // We do not blend on image border
            //bool isImageEdge = (destX == 0 || destX == (int)width - 1 || destY == 0 || destY == (int)height - 1);

            float weight = 1.0f;
            
            //if (!isImageEdge) {
            weight = std::min(std::max(distToEdge / featherMargin, 0.1f), 1.0f);
            //}

            textureData[idx + 0] = std::fma(weight, r, textureData[idx + 0]);
            textureData[idx + 1] = std::fma(weight, g, textureData[idx + 1]);
            textureData[idx + 2] = std::fma(weight, b, textureData[idx + 2]);
            textureData[idx + 3] += weight;
            //textureData[3 * idx + 0] = r;
            //textureData[3 * idx + 1] = g;
            //textureData[3 * idx + 2] = b;
        }
    }

    context->Unmap(staging, 0);
    staging->Release();

    return true;
}


bool FrameExport::AppendTile(
    const std::vector<float>& tileData,
    uint32_t tileWidth, uint32_t tileHeight,
    std::vector<float>& imageData,
    std::vector<float>& imageAlpha,
    uint32_t width, uint32_t height,
    int startX, int startY)
{
    // --- CLIPPING LOGIC FOR ALL 4 SIDES ---

    // 1. Find the intersection in Image Space
    // The leftmost pixel we can copy is either 0 or startX, whichever is further right
    int intersectLeft = std::max(0, startX);
    // The rightmost pixel is either image width or the end of the tile, whichever is further left
    int intersectRight = std::min((int)width, (int)(startX + tileWidth));

    int intersectTop = std::max(0, startY);
    int intersectBottom = std::min((int)height, (int)(startY + tileHeight));

    // If there is no overlap, just return true (nothing to do)
    if (intersectLeft >= intersectRight || intersectTop >= intersectBottom) {
        return true;
    }

    // 2. Map Image Space intersection back to Tile Space (Source Texture coordinates)
    // Example: if startX is -10, and intersectLeft is 0, we must start reading at tile pixel 10.
    int srcXStart = intersectLeft - startX;
    int srcYStart = intersectTop - startY;
    int srcXEnd = intersectRight - startX;
    int srcYEnd = intersectBottom - startY;

    // --- CLIPPING LOGIC END ---
    const float featherMargin = std::min(tileWidth, tileHeight) / 2.f;

    // Loop through the source texture using our calculated tile-space boundaries
    for (int srcY = srcYStart; srcY < srcYEnd; srcY++) {
        for (int srcX = srcXStart; srcX < srcXEnd; srcX++) {
            // Calculate destination index using the same relative offset
            // Destination X = startX + source X
            const int dstX = startX + srcX;
            const int dstY = startY + srcY;

            const int srcIdx = srcY * tileWidth + srcX;
            const int dstIdx = dstY * width + dstX;
            
            float feather = tileWidth * 0.25f;

            float dx = std::min((float)srcX, (float)(tileWidth - 1 - srcX));
            float dy = std::min((float)srcY, (float)(tileHeight - 1 - srcY));

            float wx = std::min(std::max(dx / feather, 0.f), 1.f);
            float wy = std::min(std::max(dy / feather, 0.f), 1.f);

            // Do not attenuate border
            if (startX <= 0) {
                wx = 1.f;
            }
            if (startX + tileWidth >= width) {
                wx = 1.f;
            }

            if (startY <= 0) {
                wy = 1.f;
            }
            if (startY + tileHeight >= height) {
                wy = 1.f;
            }

            // Hann window for smooth blending
            wx = 0.5f - 0.5f * std::cos(wx * M_PI);
            wy = 0.5f - 0.5f * std::cos(wy * M_PI);

            const float weight = wx * wy;

            imageData[3 * dstIdx + 0] = std::fma(weight, tileData[3 * srcIdx + 0], imageData[3 * dstIdx + 0]);
            imageData[3 * dstIdx + 1] = std::fma(weight, tileData[3 * srcIdx + 1], imageData[3 * dstIdx + 1]);
            imageData[3 * dstIdx + 2] = std::fma(weight, tileData[3 * srcIdx + 2], imageData[3 * dstIdx + 2]);
            imageAlpha[dstIdx] += weight;
        }
    }

    return true;
}


void FrameExport::WriteEXRJob(std::shared_ptr<EXRJob> job)
{
    std::thread([job]() {
        EXRHeader header;
        InitEXRHeader(&header);

        EXRImage image;
        InitEXRImage(&image);

        image.num_channels = 3;

        float* image_ptr[3];
        image_ptr[0] = job->images[0].data(); // B
        image_ptr[1] = job->images[1].data(); // G
        image_ptr[2] = job->images[2].data(); // R

        image.images = (unsigned char**)image_ptr;
        image.width = job->width;
        image.height = job->height;

        header.num_channels = 3;

        header.channels =
            (EXRChannelInfo*)malloc(
                sizeof(EXRChannelInfo) * 3);

        strcpy(header.channels[0].name, "B");
        strcpy(header.channels[1].name, "G");
        strcpy(header.channels[2].name, "R");

        header.pixel_types =
            (int*)malloc(sizeof(int) * 3);

        header.requested_pixel_types =
            (int*)malloc(sizeof(int) * 3);

        for (int i = 0; i < 3; i++) {
            header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
            header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;
        }

        const char* err = nullptr;

        int ret = SaveEXRImageToFile(
            &image,
            &header,
            job->filename.c_str(),
            &err);

        free(header.channels);
        free(header.pixel_types);
        free(header.requested_pixel_types);

        if (ret != TINYEXR_SUCCESS) {
            if (err) {
                FreeEXRErrorMessage(err);
            }
        }
    }).detach();
}


void FrameExport::WriteHDJob(std::shared_ptr<HDJob> job)
{
    std::thread([job]() {
        std::shared_ptr<TiffJob> tiffJob = std::make_shared<TiffJob>();

        tiffJob->filename = job->filename;
        tiffJob->width = job->tileWidth * 5;
        tiffJob->height = job->tileHeight * 5;

        tiffJob->image = std::vector<float>(tiffJob->width * tiffJob->height * 3, 0.0f);
        std::vector<float> alpha(tiffJob->width * tiffJob->height, 0.0f);

        for (size_t iTile = 0; iTile < job->tilesPos.size(); iTile++) {
            const int tileX = job->tilesPos[iTile].first;
            const int tileY = job->tilesPos[iTile].second;

            const std::vector<float>& tileImage = job->tilesImage[iTile];

            AppendTile(
                tileImage,
                job->tileWidth,
                job->tileHeight,
                tiffJob->image,
                alpha,
                tiffJob->width,
                tiffJob->height,
                (1 + tileX) * job->tileWidth / 2.f,
                (1 + tileY) * job->tileHeight / 2.f
            );
        }

        // Normalize the combined image by the alpha values
        for (size_t i = 0; i < alpha.size(); i++) {
            const float a = alpha[i];

            if (a > 0.0f) {
                tiffJob->image[3 * i + 0] /= a;
                tiffJob->image[3 * i + 1] /= a;
                tiffJob->image[3 * i + 2] /= a;
            }
        }

        WriteTIFFJob(tiffJob);
    }).detach();
}


bool FrameExport::SaveR11G11B10TextureAsEXR(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* srcTex,
    const char* filename)
{
    D3D11_TEXTURE2D_DESC desc;
    srcTex->GetDesc(&desc);

    if (desc.Format != DXGI_FORMAT_R11G11B10_FLOAT) {
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* staging = nullptr;

    HRESULT hr = device->CreateTexture2D(
        &stagingDesc,
        nullptr,
        &staging);

    if (FAILED(hr)) {
        return false;
    }

    context->CopyResource(staging, srcTex);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);

    if (FAILED(hr)) {
        staging->Release();
        return false;
    }

    auto job = std::make_shared<EXRJob>();

    job->filename = filename;
    job->width = (int)desc.Width;
    job->height = (int)desc.Height;

    job->images[0].resize(job->width * job->height);
    job->images[1].resize(job->width * job->height);
    job->images[2].resize(job->width * job->height);

    for (int y = 0; y < job->height; y++)
    {
        const uint32_t* row =
            (const uint32_t*)
            ((const uint8_t*)mapped.pData +
                y * mapped.RowPitch);

        for (int x = 0; x < job->width; x++) {
            float r, g, b;
            DecodeR11G11B10Float(row[x], r, g, b);

            const int idx = y * job->width + x;
            job->images[0][idx] = b;
            job->images[1][idx] = g;
            job->images[2][idx] = r;
        }
    }

    context->Unmap(staging, 0);
    staging->Release();

    WriteEXRJob(job);

    return true;
}


void FrameExport::WriteTIFFJob(std::shared_ptr<TiffJob> job)
{
    std::thread([job]() {
        TIFF* tif = TIFFOpen(job->filename.c_str(), "w");

        if (!tif) {
            return;
        }

        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, job->width);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, job->height);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 32);
        TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, 0));

        // Convert from RGBA to RGB
        for (int y = 0; y < job->height; y++) {
            const float* row = &job->image[y * job->width * 3];

            TIFFWriteScanline(tif, (void*)row, y, 0);
        }


        TIFFClose(tif);
    }).detach();
}


bool FrameExport::SaveR11G11B10TextureAsTIFF(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* srcTex,
    const char* filename)
{
    auto job = std::make_shared<TiffJob>();

    GetTexture(
        device,
        context,
        srcTex,
        job->image,
        job->width,
        job->height
    );

    job->filename = filename;

    WriteTIFFJob(job);

    return true;
}