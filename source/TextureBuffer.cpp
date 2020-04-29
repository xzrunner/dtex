#include "dtex/TextureBuffer.h"
#include "dtex/TexRenderer.h"
#include "dtex/TexBufBlock.h"
#include "dtex/TexBufPreNode.h"
#include "dtex/TexRenderer.h"
#include "dtex/TexPacker.h"

#include <unirender/Device.h>
#include <unirender/Texture.h>
#include <unirender/TextureDescription.h>

#include <assert.h>

namespace dtex
{

TextureBuffer::TextureBuffer(const ur::Device& dev, int width, int height)
{
    InitTexture(dev, width, height);
    InitBlocks(width, height);
}

void TextureBuffer::LoadStart()
{
    m_loadable++;
}

void TextureBuffer::Load(const ur::TexturePtr& tex, const Rect& r, uint64_t key,
                         int padding, int extrude, int src_extrude)
{
	if (!tex || tex->GetWidth() <= 0 || tex->GetHeight() <= 0) {
		return;
	}

    int extend = padding + extrude;
	int w = r.xmax - r.xmin + extend * 2,
		h = r.ymax - r.ymin + extend * 2;
	if ((w <= m_block_w && h <= m_block_h) ||
		(w <= m_block_h && h <= m_block_w)) {
		;
	} else {
		return;
	}

	int block_id;
	if (Query(key, block_id)) {
		return;
	}

	m_prenodes.insert(TexBufPreNode(tex, r, key, padding, extrude, src_extrude));
}

void TextureBuffer::LoadFinish(ur::Context& ctx, TexRenderer& rd)
{
	if (--m_loadable > 0 || m_prenodes.empty()) {
		return;
	}

	try {
		// insert
		std::list<TexBufDrawTask> drawlist;
		std::list<std::shared_ptr<TexBufBlock>> clearlist;

		auto itr_prenode = m_prenodes.begin();
		for (int i = 0; itr_prenode != m_prenodes.end(); ++itr_prenode, ++i) {
			int block_clear_idx = -1;
			InsertNode(*itr_prenode, drawlist, clearlist, block_clear_idx);
			//if (block_clear_idx != -1) {
			//	CacheAPI::OnClearSymBlock(block_clear_idx);
			//}
		}

		// draw clear
		clearlist.sort();
		clearlist.unique();
		if (clearlist.size() == BLOCK_X_SZ * BLOCK_Y_SZ) {
			// clear all texture
			rd.ClearTex(ctx, m_tex, 0, 0, 1, 1);
		} else {
			auto itr_clearlist = clearlist.begin();
			for (; itr_clearlist != clearlist.end(); ++itr_clearlist) {
				ClearBlockTex(ctx, rd, **itr_clearlist);
			}
		}

		// draw
		drawlist.sort();
		auto itr_drawlist = drawlist.begin();
		for (; itr_drawlist != drawlist.end(); ++itr_drawlist) {
			if (!itr_drawlist->Draw(ctx, rd)) {
				throw std::exception();
			}
		}
        rd.Flush(ctx);

		// clear
		m_prenodes.clear();
	} catch (std::exception&) {
		for (int i = 0, n = BLOCK_X_SZ * BLOCK_Y_SZ; i < n; ++i) {
			m_blocks[i]->Clear();
		}

		//// -1 for all
		//CacheAPI::OnClearSymBlock(-1);

		m_nodes.clear();

		m_clear_block_idx = 0;
	}
}

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

void TextureBuffer::ClearBlockData()
{
	m_blocks[m_clear_block_idx]->Clear();;
	//CacheAPI::OnClearSymBlock(m_clear_block_idx);

	++m_clear_block_idx;
	if (m_clear_block_idx >= BLOCK_X_SZ * BLOCK_Y_SZ) {
		m_clear_block_idx -= BLOCK_X_SZ * BLOCK_Y_SZ;
	}
}

void TextureBuffer::ClearBlockTex(ur::Context& ctx, const TexRenderer& rd, const TexBufBlock& b)
{
	float offx = static_cast<float>(b.OffX()),
		  offy = static_cast<float>(b.OffY());
	int tex_w = m_tex->GetWidth(),
		tex_h = m_tex->GetHeight();
	float xmin = offx / tex_w,
		  ymin = offy / tex_h,
		  xmax = (offx + m_block_w) / tex_w,
		  ymax = (offy + m_block_h) / tex_h;
	rd.ClearTex(ctx, m_tex, xmin, ymin, xmax, ymax);
}

bool TextureBuffer::InsertNode(const TexBufPreNode& prenode, std::list<TexBufDrawTask>& drawlist,
                               std::list<std::shared_ptr<TexBufBlock>>& clearlist, int& clear_block_idx)
{
	clear_block_idx = -1;

	int extend = prenode.Padding() + prenode.Extrude();

    std::shared_ptr<TexBufBlock> block = nullptr;
    Quad q;
	for (int i = 0, n = BLOCK_X_SZ * BLOCK_Y_SZ; i < n; ++i)
    {
        q = m_blocks[i]->Insert(prenode, extend);
        if (q.rect.IsValid()) {
            block = m_blocks[i];
            break;
        }
	}

	if (!block)
	{
		auto cleared = m_blocks[m_clear_block_idx];
		ClearBlockData();
		clear_block_idx = m_clear_block_idx;

		clearlist.push_back(cleared);
		// update drawlist
		for (auto itr = drawlist.begin(); itr != drawlist.end(); )
		{
			if (itr->GetBlock() == cleared) {
				itr = drawlist.erase(itr);
			} else {
				++itr;
			}
		}

        q = cleared->Insert(prenode, extend);
        if (q.rect.IsValid()) {
            block = cleared;
        } else {
            return false;
        }
	}

	TexBufNode node(prenode.Key(), m_tex, q);
	m_nodes.push_back(node);

	int src_extrude = prenode.SrcExtrude();

	Rect src_r = prenode.GetRect();
	src_r.xmin -= src_extrude;
	src_r.ymin -= src_extrude;
	src_r.xmax += src_extrude;
	src_r.ymax += src_extrude;

	Rect dst_r;
	dst_r.xmin = q.rect.xmin - src_extrude;
	dst_r.ymin = q.rect.ymin - src_extrude;
	dst_r.xmax = q.rect.xmax + src_extrude;
	dst_r.ymax = q.rect.ymax + src_extrude;

	drawlist.push_back(TexBufDrawTask(m_tex, block, prenode, src_r, q));

	block->Insert(node.Key(), m_nodes.size() - 1);

	return true;
}

}