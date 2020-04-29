#pragma once

#include "dtex/Utility.h"
#include "dtex/TexBufNode.h"
#include "dtex/TexBufPreNode.h"
#include "dtex/TexBufDrawTask.h"

#include <unirender/typedef.h>

#include <list>
#include <set>
#include <vector>

namespace ur { class Device; class Context; }

namespace dtex
{

class TexBufBlock;
class TexBufPreNode;
class TexRenderer;

class TextureBuffer
{
public:
    TextureBuffer(const ur::Device& dev, int width, int height);

    void LoadStart();
    void Load(const ur::TexturePtr& tex, const Rect& r, uint64_t key,
        int padding = 0, int extrude = 0, int src_extrude = 0);
    void LoadFinish(ur::Context& ctx, TexRenderer& rd);

    const TexBufNode* Query(uint64_t key, int& block_id) const;

    auto GetTexture() const { return m_tex; }

private:
    void InitTexture(const ur::Device& dev, int width, int height);
    void InitBlocks(int width, int height);

    void ClearBlockData();

    void ClearBlockTex(ur::Context& ctx, const TexRenderer& rd, const TexBufBlock& b);

    bool InsertNode(const TexBufPreNode& node, std::list<TexBufDrawTask>& drawlist,
        std::list<std::shared_ptr<TexBufBlock>>& clearlist, int& clear_block_idx);

private:
    static const int BLOCK_X_SZ = 2;
    static const int BLOCK_Y_SZ = 2;

private:
    int m_loadable = 0;

    ur::TexturePtr m_tex = nullptr;
    std::shared_ptr<TexBufBlock> m_blocks[BLOCK_X_SZ * BLOCK_Y_SZ];
    int m_block_w = 0, m_block_h = 0;

    std::set<TexBufPreNode, TexBufPreNodeKeyCmp> m_prenodes;
    std::vector<TexBufNode> m_nodes;

    int m_clear_block_idx = 0;

}; // TextureBuffer

}