#pragma once

#include "dtex/TexBufPreNode.h"

#include <unirender/typedef.h>

#include <unordered_map>

namespace dtex
{

class TexPacker;

class TexBufBlock
{
public:
	TexBufBlock(const ur::TexturePtr& tex, int x, int y, int w, int h);
	~TexBufBlock();

	bool operator == (const TexBufBlock& block) const {
		return m_x == block.m_x && m_y == block.m_y;
	}
	bool operator < (const TexBufBlock& block) const {
		return m_x < block.m_x || (m_x == block.m_x && m_y < block.m_y);
	}

	void Clear();

	int Query(uint64_t key) const;

	Quad Insert(const TexBufPreNode& node, int extend);
	Quad InsertShadow(const TexBufPreNode& node, int extend);
	void Insert(uint64_t key, int val);

	bool HasShadow() const { return static_cast<bool>(m_shadow); }
	void CommitShadow();
	void AbortShadow();

	using Lookup = std::unordered_map<uint64_t, int>;
	Lookup PrepareLookup(bool replace, size_t additional) const;
	void CommitLookup(Lookup&& lookup) noexcept;
	void InvalidateLookup() noexcept;

	int OffX() const { return m_x; }
	int OffY() const { return m_y; }

private:
	Quad PackInto(TexPacker& packer, const TexBufPreNode& node, int extend);

    ur::TexturePtr m_tex = nullptr;

	int m_x, m_y;
	int m_w = 0, m_h = 0;

    std::unique_ptr<TexPacker> m_tp = nullptr;
    std::unique_ptr<TexPacker> m_shadow = nullptr;
	Lookup m_lut;

}; // TexBufBlock

}
