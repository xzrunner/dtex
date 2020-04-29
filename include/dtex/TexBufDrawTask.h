#pragma once

#include "dtex/Utility.h"

#include <unirender2/typedef.h>

namespace ur2 { class Context; }

namespace dtex
{

class TexBufBlock;
class TexBufPreNode;
class TexRenderer;

class TexBufDrawTask
{
public:
	TexBufDrawTask(const ur2::TexturePtr& tex, const std::shared_ptr<TexBufBlock>& block,
        const TexBufPreNode& pn, const Rect& src, const Quad& dst);

    bool operator == (const TexBufDrawTask& node) const;
    bool operator < (const TexBufDrawTask& node) const;

	bool Draw(ur2::Context& ctx, TexRenderer& rd) const;

	auto GetBlock() const { return m_block; }

private:
	bool DrawExtrude(ur2::Context& ctx, TexRenderer& rd, const ur2::TexturePtr& src,
        const Rect& src_r, const Rect& dst_r, bool rotate, int extrude) const;

private:
    ur2::TexturePtr m_tex = nullptr;
    std::shared_ptr<TexBufBlock> m_block = nullptr;

    const TexBufPreNode& m_pn;
	Rect m_src, m_dst;
	bool m_rotate;

}; // TexBufDrawTask

}