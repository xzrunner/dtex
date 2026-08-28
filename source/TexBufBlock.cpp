#include "dtex/TexBufBlock.h"
#include "dtex/TexPacker.h"

#include <stdexcept>

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
	, m_w(w)
	, m_h(h)
{
    m_tp = std::make_unique<TexPacker>(w, h, MAX_BLOCK_PRELOAD_COUNT);
}

TexBufBlock::~TexBufBlock() = default;

void TexBufBlock::Clear()
{
    m_tp->Clear();
	m_shadow.reset();
	m_lut.clear();
}

int TexBufBlock::Query(uint64_t key) const
{
    auto itr = m_lut.find(key);
    return itr == m_lut.end() ? -1 : itr->second;
}

Quad TexBufBlock::PackInto(TexPacker& packer, const TexBufPreNode& node, int extend)
{
    Quad ret;

	int w = node.Width() + extend * 2,
		h = node.Height() + extend * 2;
    ret = packer.Add(w, h, true);
    if (!ret.rect.IsValid()) {
        return ret;
    }

	const int xmin = static_cast<int>(ret.rect.xmin) + extend + m_x;
	const int ymin = static_cast<int>(ret.rect.ymin) + extend + m_y;
	const int xmax = static_cast<int>(ret.rect.xmax) - extend + m_x;
	const int ymax = static_cast<int>(ret.rect.ymax) - extend + m_y;
	if (xmin < std::numeric_limits<int16_t>::min() ||
		ymin < std::numeric_limits<int16_t>::min() ||
		xmax > std::numeric_limits<int16_t>::max() ||
		ymax > std::numeric_limits<int16_t>::max() ||
		xmax <= xmin || ymax <= ymin) {
		ret.rect.MakeInvalid();
		return ret;
	}
	ret.rect.xmin = static_cast<int16_t>(xmin);
	ret.rect.ymin = static_cast<int16_t>(ymin);
	ret.rect.xmax = static_cast<int16_t>(xmax);
	ret.rect.ymax = static_cast<int16_t>(ymax);

	return ret;
}

Quad TexBufBlock::Insert(const TexBufPreNode& node, int extend)
{
    return PackInto(*m_tp, node, extend);
}

Quad TexBufBlock::InsertShadow(const TexBufPreNode& node, int extend)
{
    if (!m_shadow) {
        m_shadow = std::make_unique<TexPacker>(m_w, m_h, MAX_BLOCK_PRELOAD_COUNT);
    }
    return PackInto(*m_shadow, node, extend);
}

void TexBufBlock::CommitShadow()
{
    if (!m_shadow) {
        return;
    }
    m_tp = std::move(m_shadow);
}

void TexBufBlock::AbortShadow()
{
    m_shadow.reset();
}

TexBufBlock::Lookup TexBufBlock::PrepareLookup(bool replace, size_t additional) const
{
    Lookup lookup = replace ? Lookup() : m_lut;
	if (additional > std::numeric_limits<size_t>::max() - lookup.size()) {
		throw std::length_error("dtex lookup size overflow");
	}
    lookup.reserve(lookup.size() + additional);
    return lookup;
}

void TexBufBlock::CommitLookup(Lookup&& lookup) noexcept
{
    m_lut.swap(lookup);
}

void TexBufBlock::InvalidateLookup() noexcept
{
	m_lut.clear();
}

void TexBufBlock::Insert(uint64_t key, int val)
{
    m_lut.insert({ key, val });
}

}
