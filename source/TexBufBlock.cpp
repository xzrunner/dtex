#include "dtex/TexBufBlock.h"
#include "dtex/TexPacker.h"

namespace
{

const int MAX_BLOCK_PRELOAD_COUNT = 1024;

}

namespace dtex
{

TexBufBlock::TexBufBlock(const ur::TexturePtr& tex, int x, int y, int w, int h)
	: m_tex(tex)
	, m_x(x)
	, m_y(y)
{
    m_tp = std::make_unique<TexPacker>(w, h, MAX_BLOCK_PRELOAD_COUNT);
}

void TexBufBlock::Clear()
{
    m_tp->Clear();
	m_lut.clear();
}

int TexBufBlock::Query(uint64_t key) const
{
    auto itr = m_lut.find(key);
    return itr == m_lut.end() ? -1 : itr->second;
}

Quad TexBufBlock::Insert(const TexBufPreNode& node, int extend)
{
    Quad ret;

	int w = node.Width() + extend * 2,
		h = node.Height() + extend * 2;
    ret = m_tp->Add(w, h, true);
    if (!ret.rect.IsValid()) {
        return ret;
    }

    ret.rect.xmin += extend;
    ret.rect.ymin += extend;
    ret.rect.xmax -= extend;
    ret.rect.ymax -= extend;

    ret.rect.xmin += m_x;
    ret.rect.xmax += m_x;
    ret.rect.ymin += m_y;
    ret.rect.ymax += m_y;

	return ret;
}

void TexBufBlock::Insert(uint64_t key, int val)
{
    m_lut.insert({ key, val });
}

}