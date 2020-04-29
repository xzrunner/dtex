#pragma once

#include "dtex/TexBufPreNode.h"

#include <unirender2/typedef.h>

#include <unordered_map>

namespace dtex
{

class TexPacker;

class TexBufBlock
{
public:
	TexBufBlock(const ur2::TexturePtr& tex, int x, int y, int w, int h);

	bool operator == (const TexBufBlock& block) const {
		return m_x == block.m_x && m_y == block.m_y;
	}
	bool operator < (const TexBufBlock& block) const {
		return m_x < block.m_x || (m_x == block.m_x && m_y < block.m_y);
	}

	void Clear();

	int Query(uint64_t key) const;

	Quad Insert(const TexBufPreNode& node, int extend);
	void Insert(uint64_t key, int val);

	int OffX() const { return m_x; }
	int OffY() const { return m_y; }

private:
    ur2::TexturePtr m_tex = nullptr;

	int m_x, m_y;

    std::unique_ptr<TexPacker> m_tp = nullptr;
    std::unordered_map<uint64_t, int> m_lut;

}; // TexBufBlock

}