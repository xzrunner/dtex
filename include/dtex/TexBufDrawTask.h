#pragma once

#include "dtex/Utility.h"

#include <unirender/typedef.h>

namespace ur { class Context; }

namespace dtex
{

class TexBufBlock;
class TexBufPreNode;
class TexRenderer;

class TexBufDrawTask
{
public:
	TexBufDrawTask(const ur::TexturePtr& tex, const std::shared_ptr<TexBufBlock>& block,
        const TexBufPreNode& pn, const Rect& src, const Quad& dst);

    bool operator == (const TexBufDrawTask& node) const;
    bool operator < (const TexBufDrawTask& node) const;

	bool Draw(ur::Context& ctx, TexRenderer& rd) const;

	auto GetBlock() const { return m_block; }

private:
	bool DrawExtrude(ur::Context& ctx, TexRenderer& rd, const ur::TexturePtr& src,
        const Rect& src_r, const Rect& dst_r, bool rotate, int extrude) const;

private:
    ur::TexturePtr m_tex = nullptr;
    std::shared_ptr<TexBufBlock> m_block = nullptr;

    const TexBufPreNode& m_pn;
	Rect m_src, m_dst;
	bool m_rotate;

}; // TexBufDrawTask

}