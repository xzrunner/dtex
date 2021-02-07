#include "dtex/PixelBuffer.h"
#include "dtex/PixBufPage.h"
#include "dtex/TextureBuffer.h"
#include "dtex/PixBufPage.h"

#include <assert.h>

namespace
{

const int MAX_NODE_SIZE = 512;
const int PADDING = 1;

}

namespace dtex
{

PixelBuffer::PixelBuffer(const ur::Device& dev, int width, int height)
    : m_width(width)
    , m_height(height)
{
    m_pages.push_back(std::make_unique<PixBufPage>(dev, width, height));
}

PixelBuffer::~PixelBuffer()
{
}

void PixelBuffer::Load(const ur::Device& dev, ur::Context& ctx, const uint32_t* bitmap,
                       int width, int height, uint64_t key)
{
	int pw = width + PADDING * 2;
	int ph = height + PADDING * 2;
	if (!bitmap ||
	    (!(pw <= m_width && ph <= m_height) &&
		 !(ph <= m_width && pw <= m_height))) {
		return;
	}
	if (Exist(key)) {
		return;
	}

	Quad dst_pos;

	int page_idx = -1;
	for (size_t i = 0, n = m_pages.size(); i < n; ++i)
    {
        dst_pos = m_pages[i]->AddToTP(pw, ph);
        if (dst_pos.rect.IsValid()) {
            page_idx = i;
            break;
        }
	}
	if (page_idx < 0)
	{
		auto page = std::make_unique<PixBufPage>(dev, m_width, m_height);
        dst_pos = page->AddToTP(pw, ph);
        assert(dst_pos.rect.IsValid());
		page_idx = m_pages.size();
		m_pages.push_back(std::move(page));
	}

	// old version: rebuild
	//if (!m_pages[page_idx]->AddToTP(pw, ph, r))
	//{
	//	Flush();
	//	RenderAPI::Flush();
	//	Clear();
	//	if (!m_pages[page_idx]->AddToTP(pw, ph, r)) {
	//		return;
	//	}
	//}

	auto r_no_padding = dst_pos.rect;
	r_no_padding.xmin += PADDING;
	r_no_padding.ymin += PADDING;
	r_no_padding.xmax -= PADDING;
	r_no_padding.ymax -= PADDING;

	Node node({ key, static_cast<size_t>(page_idx), r_no_padding });
	m_all_nodes.insert({ key, node });
	m_new_nodes.push_back(node);

	m_pages[page_idx]->UpdateBitmap(ctx, bitmap, width, height, r_no_padding, dst_pos.rect);
}

bool PixelBuffer::Flush(ur::Context& ctx, TextureBuffer& tex_buf, TexRenderer& rd)
{
	bool dirty = false;

	if (m_new_nodes.empty()) {
		return false;
	}

	//bool bind_fbo = false;
	for (auto& p : m_pages) {
		if (p->UploadTexture(ctx)) {
			dirty = true;
			//bind_fbo = true;
		}
	}

    // update to texture buffer
    tex_buf.LoadStart();
	for (auto& n : m_new_nodes) {
        auto tex = m_pages[n.page]->GetTexture();
        tex_buf.Load(tex, n.region, n.key, 1, 0);
	}
    tex_buf.LoadFinish(ctx, rd);
	dirty = true;

	m_new_nodes.clear();

//	if (bind_fbo) {
//		RenderAPI::GetRenderContext()->UnbindPixelBuffer();
//	}

	return dirty;
}

bool PixelBuffer::QueryAndInsert(uint64_t key, float* texcoords, ur::TexturePtr& tex) const
{
	auto itr = m_all_nodes.find(key);
	if (itr == m_all_nodes.end()) {
		return false;
	}

	auto& node = itr->second;
	m_new_nodes.push_back(node);

    tex = m_pages[node.page]->GetTexture();

	const Rect& r = node.region;
	float xmin = r.xmin / static_cast<float>(m_width),
		  ymin = r.ymin / static_cast<float>(m_height),
		  xmax = r.xmax / static_cast<float>(m_width),
		  ymax = r.ymax / static_cast<float>(m_height);
	texcoords[0] = xmin; texcoords[1] = ymin;
	texcoords[2] = xmax; texcoords[3] = ymin;
	texcoords[4] = xmax; texcoords[5] = ymax;
	texcoords[6] = xmin; texcoords[7] = ymax;

	return true;
}

//void PixelBuffer::GetFirstPageTexInfo(int& id, size_t& w, size_t& h) const
//{
//	assert(!m_pages.empty());
//	auto& p = m_pages.front();
//	id = p->GetTexID();
//	w = p->GetWidth();
//	h = p->GetHeight();
//}
//
//bool PixelBuffer::QueryRegion(uint64_t key, ur::TexturePtr& tex, int& xmin, int& ymin, int& xmax, int& ymax) const
//{
//	auto itr = m_all_nodes.find(key);
//	if (itr == m_all_nodes.end()) {
//		return false;
//	}
//
//	auto& r = itr->second.region;
//	xmin = r.xmin;
//	ymin = r.ymin;
//	xmax = r.xmax;
//	ymax = r.ymax;
//
//	tex = m_pages[itr->second.page]->GetTexture();
//
//	return true;
//}

ur::TexturePtr PixelBuffer::GetFirstPageTex() const
{
	return m_pages.empty() ? nullptr : m_pages[0]->GetTexture();
}

}