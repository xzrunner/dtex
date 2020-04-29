#pragma once

#include "dtex/Utility.h"

#include <unirender/typedef.h>

namespace dtex
{

class TexBufPreNode
{
public:
	TexBufPreNode(const ur::TexturePtr& tex, const Rect& r, uint64_t key,
        int padding, int extrude, int src_extrude);

	bool operator == (const TexBufPreNode& node) const { return m_key == node.m_key; }
	bool operator < (const TexBufPreNode& node) const { return MaxEdge() > node.MaxEdge(); }

    auto GetTexture() const { return m_tex; }

	int Width() const { return m_rect.xmax - m_rect.xmin; }
	int Height() const { return m_rect.ymax - m_rect.ymin; }

	uint64_t Key() const { return m_key; }

	const Rect& GetRect() const { return m_rect; }

	int Padding() const { return m_padding; }
	int Extrude() const { return m_extrude; }
	int SrcExtrude() const { return m_src_extrude; }

private:
	int Area() const { return Width() * Height(); }
	int MaxEdge() const {
		int w = Width(), h = Height();
		return w >= h ? w : h;
	}

private:
    ur::TexturePtr m_tex = nullptr;
	Rect m_rect;

	uint64_t m_key;

	int m_padding, m_extrude;
	int m_src_extrude;

}; // TexBufPreNode

struct TexBufPreNodeKeyCmp
{
	bool operator () (const TexBufPreNode& lhs, const TexBufPreNode& rhs) const {
		return lhs.Key() < rhs.Key();
	}
};

}