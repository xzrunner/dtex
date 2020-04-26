#pragma once

#include "dtex/Utility.h"
#include "dtex/TexPacker.h"

#include <unirender2/typedef.h>

namespace ur2 { class Device; class Context; class WritePixelBuffer; }

namespace dtex3
{

class TexPacker;

class PixelBufferPage
{
public:
    PixelBufferPage(const ur2::Device& dev, size_t width, size_t height);

    bool AddToTP(size_t width, size_t height, Rect& ret);

    void Clear();

    auto GetTexture() const { return m_tex; }
    //size_t GetWidth() const { return m_width; }
    //size_t GetHeight() const { return m_height; }

    void UpdateBitmap(ur2::Context& ctx, const uint32_t* bitmap,
        int width, int height, const Rect& pos, const Rect& dirty_r);
    bool UploadTexture(ur2::Context& ctx);

private:
    void InitDirtyRect();
    void UpdateDirtyRect(const Rect& r);

private:
    size_t m_width, m_height;

    ur2::TexturePtr m_tex = nullptr;
    std::unique_ptr<TexPacker> m_tp = nullptr;

    std::shared_ptr<ur2::WritePixelBuffer> m_pbuf = nullptr;

    Rect m_dirty_rect;

}; // PixelBufferPage

}