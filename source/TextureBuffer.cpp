#include "dtex/TextureBuffer.h"
#include "dtex/TexRenderer.h"
#include "dtex/TexBufBlock.h"
#include "dtex/TexBufPreNode.h"
#include "dtex/TexPacker.h"

#include <unirender/Device.h>
#include <unirender/Texture.h>
#include <unirender/TextureDescription.h>

#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace dtex
{

TextureBuffer::TextureBuffer(const ur::Device& dev, int width, int height)
{
    InitTexture(dev, width, height);
    InitBlocks(width, height);
}

bool TextureBuffer::LoadStart() noexcept
{
    if (m_loadable == std::numeric_limits<int>::max()) {
        return false;
    }
    m_loadable++;
    return true;
}

bool TextureBuffer::AbortLoad() noexcept
{
    if (m_loadable <= 0) {
        return false;
    }
    --m_loadable;
    return true;
}

bool TextureBuffer::Load(const ur::TexturePtr& tex, const Rect& r, uint64_t key,
                         int padding, int extrude, int src_extrude)
{
	if (m_loadable <= 0 || !tex || tex->GetWidth() <= 0 || tex->GetHeight() <= 0 ||
		!m_tex || m_tex->GetWidth() <= 0 || m_tex->GetHeight() <= 0 ||
		m_tex->GetWidth() > std::numeric_limits<int16_t>::max() ||
		m_tex->GetHeight() > std::numeric_limits<int16_t>::max() ||
		m_block_w <= 0 || m_block_h <= 0) {
		return false;
	}
	if (padding < 0 || extrude < 0 || src_extrude < 0 ||
		r.xmin < 0 || r.ymin < 0 || r.xmax <= r.xmin || r.ymax <= r.ymin ||
		r.xmax > tex->GetWidth() || r.ymax > tex->GetHeight()) {
		return false;
	}
	if (src_extrude > r.xmin || src_extrude > r.ymin ||
		static_cast<int64_t>(r.xmax) + src_extrude > tex->GetWidth() ||
		static_cast<int64_t>(r.ymax) + src_extrude > tex->GetHeight() ||
		static_cast<int64_t>(r.xmax) + src_extrude > std::numeric_limits<int16_t>::max() ||
		static_cast<int64_t>(r.ymax) + src_extrude > std::numeric_limits<int16_t>::max()) {
		return false;
	}

	const int64_t extend = static_cast<int64_t>(padding) + extrude;
	const int64_t w = static_cast<int64_t>(r.xmax) - r.xmin + extend * 2;
	const int64_t h = static_cast<int64_t>(r.ymax) - r.ymin + extend * 2;
	if (w <= 0 || h <= 0 ||
		!((w <= m_block_w && h <= m_block_h) ||
		  (w <= m_block_h && h <= m_block_w))) {
		return false;
	}

	int block_id;
	if (Query(key, block_id)) {
		return true;
	}

	m_prenodes.insert(TexBufPreNode(tex, r, key, padding, extrude, src_extrude));
	return true;
}

bool TextureBuffer::HasPendingKey(uint64_t key) const
{
	for (const auto& pending : m_pending_nodes) {
		if (pending.node.Key() == key) {
			return true;
		}
	}
	return false;
}

bool TextureBuffer::HasPendingOnBlock(const std::shared_ptr<TexBufBlock>& block) const
{
	for (const auto& pending : m_pending_nodes) {
		if (pending.block == block) {
			return true;
		}
	}
	return false;
}

bool TextureBuffer::IsStagedBlock(const std::shared_ptr<TexBufBlock>& block) const
{
	for (const auto& staged : m_pending_clears) {
		if (staged == block) {
			return true;
		}
	}
	return false;
}

std::shared_ptr<TexBufBlock> TextureBuffer::PickEvictVictim()
{
	const int n = BLOCK_X_SZ * BLOCK_Y_SZ;
	for (int i = 0; i < n; ++i) {
		auto candidate = m_blocks[m_clear_block_idx];
		m_clear_block_idx = (m_clear_block_idx + 1) % n;
		if (HasPendingOnBlock(candidate) || IsStagedBlock(candidate)) {
			continue;
		}
		return candidate;
	}
	return nullptr;
}

void TextureBuffer::QueueDraw(const TexBufPreNode& prenode, const std::shared_ptr<TexBufBlock>& block, const Quad& q) noexcept
{
	TexBufNode node(prenode.Key(), m_tex, q);
	int src_extrude = prenode.SrcExtrude();
	Rect src_r = prenode.GetRect();
	src_r.xmin = static_cast<int16_t>(static_cast<int>(src_r.xmin) - src_extrude);
	src_r.ymin = static_cast<int16_t>(static_cast<int>(src_r.ymin) - src_extrude);
	src_r.xmax = static_cast<int16_t>(static_cast<int>(src_r.xmax) + src_extrude);
	src_r.ymax = static_cast<int16_t>(static_cast<int>(src_r.ymax) + src_extrude);
	m_pending_tasks.emplace_back(m_tex, block, prenode, src_r, q);
	m_pending_nodes.emplace_back(block, node);
}

bool TextureBuffer::PackUnpackedPrenodes()
{
	if (m_prenodes.size() > std::numeric_limits<size_t>::max() - m_pending_nodes.size() ||
		m_prenodes.size() > std::numeric_limits<size_t>::max() - m_pending_tasks.size() ||
		m_prenodes.size() == std::numeric_limits<size_t>::max()) {
		return false;
	}
	// Reserve every allocation made by QueueDraw before touching a packer. From
	// this point QueueDraw only moves/copies noexcept value types into capacity.
	m_pending_nodes.reserve(m_pending_nodes.size() + m_prenodes.size());
	m_pending_tasks.reserve(m_pending_tasks.size() + m_prenodes.size());
	m_pending_clears.reserve(BLOCK_X_SZ * BLOCK_Y_SZ);

	const size_t max_passes = m_prenodes.size() + 1;
	for (size_t pass = 0; pass < max_passes; ++pass) {
		bool progress = false;
		for (auto itr_prenode = m_prenodes.begin(); itr_prenode != m_prenodes.end(); ++itr_prenode) {
			int block_id = -1;
			if (Query(itr_prenode->Key(), block_id) || HasPendingKey(itr_prenode->Key())) {
				continue;
			}
			const PackStatus status = InsertNode(*itr_prenode);
			if (status == PackStatus::Failed) {
				return false;
			}
			if (status == PackStatus::Packed) {
				progress = true;
			}
		}
		if (!progress) {
			break;
		}
	}
	return true;
}

bool TextureBuffer::LoadFinish(ur::Context& ctx, TexRenderer& rd)
{
	if (m_loadable <= 0) {
		return false;
	}
	if (--m_loadable > 0) {
		return true;
	}

	bool eviction_clear_attempted = false;
	try {
		if (m_prenodes.empty() && m_pending_nodes.empty()) {
			return rd.Flush(ctx);
		}

		if (!PackUnpackedPrenodes()) {
			rd.DiscardPending();
			return false;
		}

		if (m_pending_nodes.empty()) {
			return true;
		}

#ifdef DTEX_ENABLE_TEST_SEAMS
		if (!rd.ConsumeFailNextFlush()) {
			return false;
		}
#endif

		auto prepared = PreparePublish();

		std::sort(m_pending_clears.begin(), m_pending_clears.end());
		m_pending_clears.erase(
			std::unique(m_pending_clears.begin(), m_pending_clears.end()),
			m_pending_clears.end());
		eviction_clear_attempted = !m_pending_clears.empty();
		if (m_pending_clears.size() == static_cast<size_t>(BLOCK_X_SZ * BLOCK_Y_SZ)) {
			if (!rd.ClearAllTex(ctx, m_tex)) {
				InvalidateEvictedLookups();
				rd.DiscardPending();
				return false;
			}
		} else {
			for (auto itr_clearlist = m_pending_clears.begin(); itr_clearlist != m_pending_clears.end(); ++itr_clearlist) {
				if (!ClearBlockTex(ctx, rd, **itr_clearlist)) {
					InvalidateEvictedLookups();
					rd.DiscardPending();
					return false;
				}
			}
		}

		std::sort(m_pending_tasks.begin(), m_pending_tasks.end());
		for (auto itr_drawlist = m_pending_tasks.begin(); itr_drawlist != m_pending_tasks.end(); ++itr_drawlist) {
			if (!itr_drawlist->Draw(ctx, rd)) {
				if (eviction_clear_attempted) {
					InvalidateEvictedLookups();
				}
				rd.DiscardPending();
				return false;
			}
		}
		if (!rd.Flush(ctx)) {
			if (eviction_clear_attempted) {
				InvalidateEvictedLookups();
			}
			rd.DiscardPending();
			return false;
		}

		CommitPublish(std::move(prepared));
		ResetPendingBatch();
		PrunePublishedPrenodes();
		return true;
	} catch (...) {
		if (eviction_clear_attempted) {
			InvalidateEvictedLookups();
		}
		rd.DiscardPending();
		return false;
	}
}

std::vector<TextureBuffer::PreparedLookup> TextureBuffer::PreparePublish()
{
	if (m_pending_nodes.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
		m_nodes.size() > static_cast<size_t>(std::numeric_limits<int>::max()) - m_pending_nodes.size()) {
		throw std::length_error("dtex node index overflow");
	}

	const size_t base = m_nodes.size();
	m_nodes.reserve(base + m_pending_nodes.size());

	std::vector<PreparedLookup> prepared;
	prepared.reserve(BLOCK_X_SZ * BLOCK_Y_SZ);
	for (const auto& block : m_blocks) {
		size_t count = 0;
		for (const auto& pending : m_pending_nodes) {
			if (pending.block == block) {
				++count;
			}
		}
		if (count == 0) {
			continue;
		}

		const bool replace = IsStagedBlock(block);
		auto lookup = block->PrepareLookup(replace, count);
		for (size_t i = 0; i < m_pending_nodes.size(); ++i) {
			const auto& pending = m_pending_nodes[i];
			if (pending.block == block) {
				lookup.insert_or_assign(
					pending.node.Key(), static_cast<int>(base + i));
			}
		}
		prepared.push_back({ block, std::move(lookup), replace });
	}

#ifdef DTEX_ENABLE_TEST_SEAMS
	if (m_fail_next_publish > 0) {
		--m_fail_next_publish;
		throw std::bad_alloc();
	}
#endif
	return prepared;
}

void TextureBuffer::CommitPublish(std::vector<PreparedLookup>&& lookups) noexcept
{
	static_assert(std::is_nothrow_move_constructible<TexBufNode>::value,
		"TexBufNode commit must not throw after GPU mutation");
	for (auto& pending : m_pending_nodes) {
		m_nodes.push_back(std::move(pending.node));
	}
	for (auto& prepared : lookups) {
		if (prepared.commit_shadow) {
			prepared.block->CommitShadow();
		}
		prepared.block->CommitLookup(std::move(prepared.lookup));
	}
}

void TextureBuffer::InvalidateEvictedLookups() noexcept
{
	// Once any eviction clear has been attempted, a later failure cannot prove
	// which victim pixels still contain their old image. Conservatively make all
	// staged victims miss Query(); the retained pending batch can be redrawn by an
	// empty LoadStart()/LoadFinish() retry without ever exposing cleared content.
	for (const auto& block : m_pending_clears) {
		if (block) {
			block->InvalidateLookup();
		}
	}
}

void TextureBuffer::PrunePublishedPrenodes()
{
	for (auto itr = m_prenodes.begin(); itr != m_prenodes.end(); ) {
		int block_id = -1;
		if (Query(itr->Key(), block_id)) {
			itr = m_prenodes.erase(itr);
		} else {
			++itr;
		}
	}
}

void TextureBuffer::ResetPendingBatch()
{
	m_pending_tasks.clear();
	m_pending_nodes.clear();
	m_pending_clears.clear();
}

#ifdef DTEX_ENABLE_TEST_SEAMS
void TextureBuffer::FailNextQueueDrawForTest(int count) noexcept
{
	m_fail_next_queue_draw = count > 0 ? count : 0;
}

void TextureBuffer::FailNextPublishForTest(int count) noexcept
{
	m_fail_next_publish = count > 0 ? count : 0;
}
#endif

const TexBufNode*
TextureBuffer::Query(uint64_t key, int& block_id) const
{
	for (int i = 0, n = BLOCK_X_SZ * BLOCK_Y_SZ; i < n; ++i)
	{
		int idx = m_blocks[i]->Query(key);
		if (idx != -1) {
			block_id = i;
			assert(idx >= 0 && static_cast<size_t>(idx) < m_nodes.size());
			return &m_nodes[idx];
		}
	}
	block_id = -1;
	return nullptr;
}

void TextureBuffer::InitTexture(const ur::Device& dev, int width, int height)
{
    ur::TextureDescription desc;
    desc.target = ur::TextureTarget::Texture2D;
    desc.width  = width;
    desc.height = height;
    desc.format = ur::TextureFormat::RGBA8;
    m_tex = dev.CreateTexture(desc);
}

void TextureBuffer::InitBlocks(int width, int height)
{
	int x = 0, y = 0;
	m_block_w = width / BLOCK_X_SZ;
	m_block_h = height / BLOCK_Y_SZ;
	int i = 0;
	for (int iy = 0; iy < BLOCK_Y_SZ; ++iy) {
		for (int ix = 0; ix < BLOCK_X_SZ; ++ix) {
			m_blocks[i++] = std::make_unique<TexBufBlock>(m_tex, x, y, m_block_w, m_block_h);
			x += m_block_w;
		}
		x = 0;
		y += m_block_h;
	}
}

bool TextureBuffer::ClearBlockTex(ur::Context& ctx, const TexRenderer& rd, const TexBufBlock& b)
{
	float offx = static_cast<float>(b.OffX()),
		  offy = static_cast<float>(b.OffY());
	int tex_w = m_tex->GetWidth(),
		tex_h = m_tex->GetHeight();
	float xmin = offx / tex_w,
		  ymin = offy / tex_h,
		  xmax = (offx + m_block_w) / tex_w,
		  ymax = (offy + m_block_h) / tex_h;
	return rd.ClearTex(ctx, m_tex, xmin, ymin, xmax, ymax);
}

TextureBuffer::PackStatus TextureBuffer::InsertNode(const TexBufPreNode& prenode)
{
#ifdef DTEX_ENABLE_TEST_SEAMS
	if (m_fail_next_queue_draw > 0) {
		--m_fail_next_queue_draw;
		throw std::bad_alloc();
	}
#endif

	const int extend = prenode.Padding() + prenode.Extrude();
	const int n = BLOCK_X_SZ * BLOCK_Y_SZ;

	for (int i = 0; i < n; ++i) {
		Quad q;
		if (IsStagedBlock(m_blocks[i])) {
			q = m_blocks[i]->InsertShadow(prenode, extend);
		} else {
			q = m_blocks[i]->Insert(prenode, extend);
		}
		if (q.rect.IsValid()) {
			QueueDraw(prenode, m_blocks[i], q);
			return PackStatus::Packed;
		}
	}

	auto victim = PickEvictVictim();
	if (!victim) {
		return PackStatus::Deferred;
	}

	const Quad q = victim->InsertShadow(prenode, extend);
	if (!q.rect.IsValid()) {
		victim->AbortShadow();
		return PackStatus::Failed;
	}
	m_pending_clears.push_back(victim);
	QueueDraw(prenode, victim, q);
	return PackStatus::Packed;
}

}
