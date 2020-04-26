#pragma once

#include "dtex/Utility.h"
#include "dtex/PixelBufferPage.h"

#include <unirender2/typedef.h>

#include <memory>
#include <vector>
#include <unordered_map>

#include <stdint.h>

namespace ur2 { class Device; class Context; }

namespace dtex3
{

class PixelBufferPage;

class PixelBuffer
{
public:
    PixelBuffer(const ur2::Device& dev, int width, int height);

    void Load(const ur2::Device& dev, ur2::Context& ctx, const uint32_t* bitmap,
        int width, int height, uint64_t key);
    bool Flush(ur2::Context& ctx);

    bool QueryAndInsert(uint64_t key, float* texcoords, ur2::TexturePtr& tex) const;
    bool Exist(uint64_t key) const { return m_all_nodes.find(key) != m_all_nodes.end(); }

    //void GetFirstPageTexInfo(int& id, size_t& w, size_t& h) const;
    //bool QueryRegion(uint64_t key, ur2::TexturePtr& tex, int& xmin, int& ymin, int& xmax, int& ymax) const;

private:
    struct Node
    {
        uint64_t key = 0;

        size_t page = 0;
        Rect   region;
    };

private:
    int m_width, m_height;

    std::vector<std::unique_ptr<PixelBufferPage>> m_pages;

    std::unordered_map<uint64_t, Node> m_all_nodes;
    mutable std::vector<Node>          m_new_nodes;

}; // PixelBuffer

}