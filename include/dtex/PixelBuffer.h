#pragma once

#include "dtex/Utility.h"
#include "dtex/PixBufPage.h"

#include <unirender/typedef.h>

#include <memory>
#include <vector>
#include <unordered_map>

#include <stdint.h>

namespace ur { class Device; class Context; }

namespace dtex
{

class PixBufPage;
class TextureBuffer;
class TexRenderer;

class PixelBuffer
{
public:
    PixelBuffer(const ur::Device& dev, int width, int height);

    void Load(const ur::Device& dev, ur::Context& ctx, const uint32_t* bitmap,
        int width, int height, uint64_t key);
    bool Flush(ur::Context& ctx, TextureBuffer& tex_buf, TexRenderer& rd);

    bool QueryAndInsert(uint64_t key, float* texcoords, ur::TexturePtr& tex) const;
    bool Exist(uint64_t key) const { return m_all_nodes.find(key) != m_all_nodes.end(); }

    //void GetFirstPageTexInfo(int& id, size_t& w, size_t& h) const;
    //bool QueryRegion(uint64_t key, ur::TexturePtr& tex, int& xmin, int& ymin, int& xmax, int& ymax) const;

private:
    struct Node
    {
        uint64_t key = 0;

        size_t page = 0;
        Rect   region;
    };

private:
    int m_width, m_height;

    std::vector<std::unique_ptr<PixBufPage>> m_pages;

    std::unordered_map<uint64_t, Node> m_all_nodes;
    mutable std::vector<Node>          m_new_nodes;

}; // PixelBuffer

}