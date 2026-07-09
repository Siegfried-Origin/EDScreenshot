#include "FrameExport.h"

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

            const int idx = y * width + x;
            textureData[3 * idx + 0] = r;
            textureData[3 * idx + 1] = g;
            textureData[3 * idx + 2] = b;
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

    if (textureData.size() < 3 * width * height) {
        textureData.resize(3 * width * height);
    }

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
            const int idx = destY * width + destX;

            textureData[3 * idx + 0] = r;
            textureData[3 * idx + 1] = g;
            textureData[3 * idx + 2] = b;
        }
    }

    context->Unmap(staging, 0);
    staging->Release();

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