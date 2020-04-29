#include "dtex/TexBufNode.h"

#include <unirender2/Texture.h>

namespace dtex
{

TexBufNode::TexBufNode(uint64_t key, const ur2::TexturePtr& dst_tex, const Quad& dst_pos)
	: m_key(key)
	, m_dst_tex(dst_tex)
{
    const float w_inv = 1.0f / dst_tex->GetWidth();
    const float h_inv = 1.0f / dst_tex->GetHeight();
	float xmin = dst_pos.rect.xmin * w_inv,
	      xmax = dst_pos.rect.xmax * w_inv,
	      ymin = dst_pos.rect.ymin * h_inv,
	      ymax = dst_pos.rect.ymax * h_inv;
	m_texcoords[0] = xmin; m_texcoords[1] = ymin;
	m_texcoords[2] = xmax; m_texcoords[3] = ymin;
	m_texcoords[4] = xmax; m_texcoords[5] = ymax;
	m_texcoords[6] = xmin; m_texcoords[7] = ymax;
	if (dst_pos.rot)
	{
		float x, y;
		x = m_texcoords[6]; y = m_texcoords[7];
		m_texcoords[6] = m_texcoords[4]; m_texcoords[7] = m_texcoords[5];
		m_texcoords[4] = m_texcoords[2]; m_texcoords[5] = m_texcoords[3];
		m_texcoords[2] = m_texcoords[0]; m_texcoords[3] = m_texcoords[1];
		m_texcoords[0] = x;           m_texcoords[1] = y;
	}
}

}