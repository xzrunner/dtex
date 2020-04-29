#include "dtex/TexBufDrawTask.h"
#include "dtex/TexBufPreNode.h"
#include "dtex/TexRenderer.h"

#include <unirender2/Texture.h>

namespace dtex
{

TexBufDrawTask::TexBufDrawTask(const ur2::TexturePtr& tex, const std::shared_ptr<TexBufBlock>& block,
                               const TexBufPreNode& pn, const Rect& src, const Quad& dst)
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

bool TexBufDrawTask::Draw(ur2::Context& ctx, TexRenderer& rd) const
{
    rd.Draw(ctx, m_pn.GetTexture(), m_src, m_tex, m_dst, m_rotate);
	if (m_pn.Extrude() != 0) {
        DrawExtrude(ctx, rd, m_pn.GetTexture(), m_src, m_dst, m_rotate, m_pn.Extrude());
	}
	return true;
}

bool TexBufDrawTask::DrawExtrude(ur2::Context& ctx, TexRenderer& rd, const ur2::TexturePtr& src_tex, const Rect& src_r,
                                 const Rect& dst_r, bool rotate, int extrude) const
{
	static const int SRC_EXTRUDE = 1;

    const auto src_w = src_tex->GetWidth();
    const auto src_h = src_tex->GetHeight();

	Rect src, dst;
	if (!rotate)
	{
		// left
		src.xmin = 0; src.xmax = SRC_EXTRUDE; src.ymin = 0; src.ymax = src_h;
		dst = dst_r; dst.xmax = dst.xmin; dst.xmin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// right
		src.xmin = src_w - SRC_EXTRUDE; src.xmax = src_w - SRC_EXTRUDE; src.ymin = 0; src.ymax = src_h;
		dst = dst_r; dst.xmin = dst.xmax; dst.xmax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// top
		src.xmin = 0; src.xmax = src_w; src.ymin = src_h - SRC_EXTRUDE; src.ymax = src_h;
		dst = dst_r; dst.ymin = dst.ymax; dst.ymax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// bottom
		src.xmin = 0; src.xmax = src_w; src.ymin = 0; src.ymax = SRC_EXTRUDE;
		dst = dst_r; dst.ymax = dst.ymin; dst.ymin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// left-top
		src.xmin = 0; src.xmax = SRC_EXTRUDE; src.ymin = src_h - SRC_EXTRUDE; src.ymax = src_h;
		dst = dst_r; dst.xmax = dst.xmin; dst.xmin -= extrude; dst.ymin = dst.ymax; dst.ymax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// right-top
		src.xmin = src_w - SRC_EXTRUDE; src.xmax = src_w; src.ymin = src_h - SRC_EXTRUDE; src.ymax = src_h;
		dst = dst_r; dst.xmin = dst.xmax; dst.xmax += extrude; dst.ymin = dst.ymax; dst.ymax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// left-bottom
		src.xmin = 0; src.xmax = SRC_EXTRUDE; src.ymin = 0; src.ymax = SRC_EXTRUDE;
		dst = dst_r; dst.xmax = dst.xmin; dst.xmin -= extrude; dst.ymax = dst.ymin; dst.ymin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// right-bottom
		src.xmin = src_w - SRC_EXTRUDE; src.xmax = src_w; src.ymin = 0; src.ymax = SRC_EXTRUDE;
		dst = dst_r; dst.xmin = dst.xmax; dst.xmax += extrude; dst.ymax = dst.ymin; dst.ymin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);
	}
	else
	{
		// left
		src.xmin = 0; src.xmax = SRC_EXTRUDE; src.ymin = 0; src.ymax = src_h;
		dst = dst_r; dst.ymin = dst.ymax; dst.ymax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// right
		src.xmin = src_w - SRC_EXTRUDE; src.xmax = src_w; src.ymin = 0; src.ymax = src_h;
		dst = dst_r; dst.ymax = dst.ymin; dst.ymin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// top
		src.xmin = 0; src.xmax = src_w; src.ymin = src_h - SRC_EXTRUDE; src.ymax = src_h;
		dst = dst_r; dst.xmin = dst.xmax; dst.xmax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// bottom
		src.xmin = 0; src.xmax = src_w; src.ymin = 0; src.ymax = SRC_EXTRUDE;
		dst = dst_r; dst.xmax = dst.xmin; dst.xmin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// left-top
		src.xmin = 0; src.xmax = SRC_EXTRUDE; src.ymin = src_h - SRC_EXTRUDE; src.ymax = src_h;
		dst = dst_r; dst.xmin = dst.xmax; dst.xmax += extrude; dst.ymin = dst.ymax; dst.ymax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// right-top
		src.xmin = src_w - SRC_EXTRUDE; src.xmax = src_w; src.ymin = src_h - SRC_EXTRUDE; src.ymax = src_h;
		dst = dst_r; dst.xmin = dst.xmax; dst.xmax += extrude; dst.ymax = dst.ymin; dst.ymin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// left-bottom
		src.xmin = 0; src.xmax = SRC_EXTRUDE; src.ymin = 0; src.ymax = SRC_EXTRUDE;
		dst = dst_r; dst.xmax = dst.xmin; dst.xmin -= extrude; dst.ymin = dst.ymax; dst.ymax += extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);

		// right-bottom
		src.xmin = src_w - SRC_EXTRUDE; src.xmax = src_w; src.ymin = 0; src.ymax = SRC_EXTRUDE;
		dst = dst_r; dst.xmax = dst.xmin; dst.xmin -= extrude; dst.ymax = dst.ymin; dst.ymin -= extrude;
		rd.Draw(ctx, src_tex, src, m_tex, dst, rotate);
	}

	return true;
}

}