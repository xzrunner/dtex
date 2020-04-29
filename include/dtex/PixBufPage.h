#pragma once

#include "dtex/Utility.h"
#include "dtex/TexPacker.h"

#include <unirender/typedef.h>

namespace ur { class Device; class Context; class WritePixelBuffer; }

namespace dtex
{

class TexPacker;

class PixBufPage
{
public:
    PixBufPage(const ur::Device& dev, size_t width, size_t height);

    Quad AddToTP(size_t width, size_t height);

    void Clear();

    auto GetTexture() const { return m_tex; }
    //size_t GetWidth() const { return m_width; }
    //size_t GetHeight() const { return m_height; }

    void UpdateBitmap(ur::Context& ctx, const uint32_t* bitmap,
        int width, int height, const Rect& pos, const Rect& dirty_r);
    bool UploadTexture(ur::Context& ctx);

private:
    void InitDirtyRect();
    void UpdateDirtyRect(const Rect& r);

private:
    size_t m_width, m_height;

    ur::TexturePtr m_tex = nullptr;
    std::unique_ptr<TexPacker> m_tp = nullptr;

    std::shared_ptr<ur::WritePixelBuffer> m_pbuf = nullptr;

    Rect m_dirty_rect;

}; // PixBufPage

}