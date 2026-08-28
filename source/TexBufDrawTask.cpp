#include "dtex/TexBufDrawTask.h"
#include "dtex/TexBufPreNode.h"
#include "dtex/TexRenderer.h"

#include <unirender/Texture.h>

namespace dtex
{

TexBufDrawTask::TexBufDrawTask(const ur::TexturePtr& tex, const std::shared_ptr<TexBufBlock>& block,
                               const TexBufPreNode& pn, const Rect& src, const Quad& dst) noexcept
	: m_tex(tex)
	, m_block(block)
	, m_pn(pn)
	, m_src(src)
	, m_dst(dst.rect)
	, m_rotate(dst.rot)
{
}

bool TexBufDrawTask::operator == (const TexBufDrawTask& node) const
{
    return m_pn.GetTexture()->GetTexID() == node.m_pn.GetTexture()->GetTexID();
}

bool TexBufDrawTask::operator < (const TexBufDrawTask& node) const
{
    return m_pn.GetTexture()->GetTexID() < node.m_pn.GetTexture()->GetTexID();
}

bool TexBufDrawTask::Draw(ur::Context& ctx, TexRenderer& rd) const
{
    if (!rd.Draw(ctx, m_pn.GetTexture(), m_src, m_tex, m_dst, m_rotate)) {
        return false;
    }
	if (m_pn.Extrude() != 0) {
        return DrawExtrude(ctx, rd, m_pn.GetTexture(), m_src, m_dst, m_rotate, m_pn.Extrude());
	}
	return true;
}

bool TexBufDrawTask::DrawExtrude(ur::Context& ctx, TexRenderer& rd, const ur::TexturePtr& src_tex, const Rect& src_r,
                                 const Rect& dst_r, bool rotate, int extrude) const
{
	static const int SRC_EXTRUDE = 1;

	const int sx0 = src_r.xmin;
	const int sy0 = src_r.ymin;
	const int sx1 = src_r.xmax;
	const int sy1 = src_r.ymax;
	const int dx0 = dst_r.xmin;
	const int dy0 = dst_r.ymin;
	const int dx1 = dst_r.xmax;
	const int dy1 = dst_r.ymax;

	auto draw = [&](int src_xmin, int src_ymin, int src_xmax, int src_ymax,
		int dst_xmin, int dst_ymin, int dst_xmax, int dst_ymax) {
		Rect src, dst;
		src.xmin = static_cast<int16_t>(src_xmin);
		src.ymin = static_cast<int16_t>(src_ymin);
		src.xmax = static_cast<int16_t>(src_xmax);
		src.ymax = static_cast<int16_t>(src_ymax);
		dst.xmin = static_cast<int16_t>(dst_xmin);
		dst.ymin = static_cast<int16_t>(dst_ymin);
		dst.xmax = static_cast<int16_t>(dst_xmax);
		dst.ymax = static_cast<int16_t>(dst_ymax);
		return rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);
	};

	if (!rotate) {
		return draw(sx0, sy0, sx0 + SRC_EXTRUDE, sy1, dx0 - extrude, dy0, dx0, dy1) &&
			draw(sx1 - SRC_EXTRUDE, sy0, sx1, sy1, dx1, dy0, dx1 + extrude, dy1) &&
			draw(sx0, sy1 - SRC_EXTRUDE, sx1, sy1, dx0, dy1, dx1, dy1 + extrude) &&
			draw(sx0, sy0, sx1, sy0 + SRC_EXTRUDE, dx0, dy0 - extrude, dx1, dy0) &&
			draw(sx0, sy1 - SRC_EXTRUDE, sx0 + SRC_EXTRUDE, sy1,
				dx0 - extrude, dy1, dx0, dy1 + extrude) &&
			draw(sx1 - SRC_EXTRUDE, sy1 - SRC_EXTRUDE, sx1, sy1,
				dx1, dy1, dx1 + extrude, dy1 + extrude) &&
			draw(sx0, sy0, sx0 + SRC_EXTRUDE, sy0 + SRC_EXTRUDE,
				dx0 - extrude, dy0 - extrude, dx0, dy0) &&
			draw(sx1 - SRC_EXTRUDE, sy0, sx1, sy0 + SRC_EXTRUDE,
				dx1, dy0 - extrude, dx1 + extrude, dy0);
	}

	return draw(sx0, sy0, sx0 + SRC_EXTRUDE, sy1, dx0, dy1, dx1, dy1 + extrude) &&
		draw(sx1 - SRC_EXTRUDE, sy0, sx1, sy1, dx0, dy0 - extrude, dx1, dy0) &&
		draw(sx0, sy1 - SRC_EXTRUDE, sx1, sy1, dx1, dy0, dx1 + extrude, dy1) &&
		draw(sx0, sy0, sx1, sy0 + SRC_EXTRUDE, dx0 - extrude, dy0, dx0, dy1) &&
		draw(sx0, sy1 - SRC_EXTRUDE, sx0 + SRC_EXTRUDE, sy1,
			dx1, dy1, dx1 + extrude, dy1 + extrude) &&
		draw(sx1 - SRC_EXTRUDE, sy1 - SRC_EXTRUDE, sx1, sy1,
			dx1, dy0 - extrude, dx1 + extrude, dy0) &&
		draw(sx0, sy0, sx0 + SRC_EXTRUDE, sy0 + SRC_EXTRUDE,
			dx0 - extrude, dy1, dx0, dy1 + extrude) &&
		draw(sx1 - SRC_EXTRUDE, sy0, sx1, sy0 + SRC_EXTRUDE,
			dx0 - extrude, dy0 - extrude, dx0, dy0);

}

}
