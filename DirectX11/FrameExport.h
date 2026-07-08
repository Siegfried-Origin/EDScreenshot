#pragma once

#if defined(_XBOX_ONE) && defined(_TITLE)
#include <d3d11_x.h>
#else
#include <d3d11_1.h>
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <thread>


class FrameExport
{
public:
    struct EXRJob
    {
        std::string filename;
        uint32_t width;
        uint32_t height;
        std::vector<float> images[3];
    };

    struct TiffJob
    {
        std::string filename;
        uint32_t width;
        uint32_t height;
        std::vector<float> image;
    };

    static float DecodeR11G11B10Component(
        uint32_t mantissa,
        uint32_t exponent,
        uint32_t mantissaBits
    );

    static void DecodeR11G11B10Float(
        uint32_t packed,
        float& r,
        float& g,
        float& b
    );

    static void GetTextureSize(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* srcTex,
        uint32_t* width, uint32_t* height
    );

    static bool GetTexture(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* srcTex,
        std::vector<float>& textureData,
        uint32_t& width, uint32_t& height
    );

    static bool AppendTextureTile(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* srcTex,
        std::vector<float>& textureData,
        uint32_t width, uint32_t height,
        uint32_t startX, uint32_t startY
    );

    static void WriteEXRJob(std::shared_ptr<EXRJob> job);
    static void WriteTIFFJob(std::shared_ptr<TiffJob> job);

    static bool SaveR11G11B10TextureAsEXR(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* srcTex,
        const char* filename
    );

    static bool SaveR11G11B10TextureAsTIFF(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        ID3D11Texture2D* srcTex,
        const char* filename
    );
};