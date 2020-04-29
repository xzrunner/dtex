#include "dtex/TexBufPreNode.h"

namespace dtex
{

TexBufPreNode::TexBufPreNode(const ur2::TexturePtr& tex, const Rect& r, uint64_t key, int padding, int extrude, int src_extrude)
	: m_tex(tex)
	, m_rect(r)
	, m_key(key)
	, m_padding(padding)
	, m_extrude(extrude)
	, m_src_extrude(src_extrude)
{
}

}