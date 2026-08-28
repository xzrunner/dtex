#pragma once

#include "dtex/Utility.h"
#include "dtex/TexBufNode.h"
#include "dtex/TexBufPreNode.h"
#include "dtex/TexBufDrawTask.h"

#include <unirender/typedef.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
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

    // LoadStart/LoadFinish are strictly paired and may be nested. LoadFinish
    // returns true when the current scope closed without a CPU/GPU failure;
    // an outermost successful finish may still leave capacity-limited keys
    // deferred. Callers must use Query() to decide which keys were published.
    bool LoadStart() noexcept;
    bool AbortLoad() noexcept;
    bool Load(const ur::TexturePtr& tex, const Rect& r, uint64_t key,
        int padding = 0, int extrude = 0, int src_extrude = 0);
    bool LoadFinish(ur::Context& ctx, TexRenderer& rd);

    const TexBufNode* Query(uint64_t key, int& block_id) const;

    auto GetTexture() const { return m_tex; }

#ifdef DTEX_ENABLE_TEST_SEAMS
    // Deterministic exception-path hooks. These are absent from production
    // builds; every DTex TU in a seam-enabled test binary must use the macro.
    void FailNextQueueDrawForTest(int count) noexcept;
    void FailNextPublishForTest(int count) noexcept;
#endif

private:
    void InitTexture(const ur::Device& dev, int width, int height);
    void InitBlocks(int width, int height);

    bool ClearBlockTex(ur::Context& ctx, const TexRenderer& rd, const TexBufBlock& b);

    enum class PackStatus { Packed, Deferred, Failed };

    PackStatus InsertNode(const TexBufPreNode& node);
    std::shared_ptr<TexBufBlock> PickEvictVictim();
    bool HasPendingOnBlock(const std::shared_ptr<TexBufBlock>& block) const;
    bool IsStagedBlock(const std::shared_ptr<TexBufBlock>& block) const;
    void QueueDraw(const TexBufPreNode& prenode, const std::shared_ptr<TexBufBlock>& block, const Quad& q) noexcept;

    struct PreparedLookup
    {
        std::shared_ptr<TexBufBlock> block;
        std::unordered_map<uint64_t, int> lookup;
        bool commit_shadow = false;
    };

    std::vector<PreparedLookup> PreparePublish();
    void CommitPublish(std::vector<PreparedLookup>&& lookups) noexcept;
    void InvalidateEvictedLookups() noexcept;
    void PrunePublishedPrenodes();
    void ResetPendingBatch();
    bool HasPendingKey(uint64_t key) const;
    bool PackUnpackedPrenodes();

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

    struct PendingNode
    {
        std::shared_ptr<TexBufBlock> block;
        TexBufNode node;

        PendingNode(std::shared_ptr<TexBufBlock> b, TexBufNode n) noexcept
            : block(std::move(b)), node(std::move(n)) {}
    };

    std::vector<TexBufDrawTask> m_pending_tasks;
    std::vector<PendingNode> m_pending_nodes;
    std::vector<std::shared_ptr<TexBufBlock>> m_pending_clears;

    int m_clear_block_idx = 0;

#ifdef DTEX_ENABLE_TEST_SEAMS
    int m_fail_next_queue_draw = 0;
    int m_fail_next_publish = 0;
#endif

}; // TextureBuffer

}
