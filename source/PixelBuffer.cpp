#include "dtex/PixelBuffer.h"
#include "dtex/PixelBufferPage.h"

#include <assert.h>

namespace
{

const int MAX_NODE_SIZE = 512;
const int PADDING = 1;

}

namespace dtex3
{

PixelBuffer::PixelBuffer(const ur2::Device& dev, int width, int height)
    : m_width(width)
    , m_height(height)
{
    m_pages.push_back(std::make_unique<PixelBufferPage>(dev, width, height));
}

void PixelBuffer::Load(const ur2::Device& dev, ur2::Context& ctx, const uint32_t* bitmap,
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

	Rect r;

	int page_idx = -1;
	for (size_t i = 0, n = m_pages.size(); i < n; ++i) {
		if (m_pages[i]->AddToTP(pw, ph, r)) {
			page_idx = i;
			break;
		}
	}
	if (page_idx < 0)
	{
		auto page = std::make_unique<PixelBufferPage>(dev, m_width, m_height);
		bool ok = page->AddToTP(pw, ph, r);
		assert(ok);
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

	auto r_no_padding = r;
	r_no_padding.xmin += PADDING;
	r_no_padding.ymin += PADDING;
	r_no_padding.xmax -= PADDING;
	r_no_padding.ymax -= PADDING;

	Node node({ key, static_cast<size_t>(page_idx), r_no_padding });
	m_all_nodes.insert({ key, node });
	m_new_nodes.push_back(node);

	m_pages[page_idx]->UpdateBitmap(ctx, bitmap, width, height, r_no_padding, r);
}

bool PixelBuffer::Flush(ur2::Context& ctx)
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

	//if (cache_to_c2)
	//{
 //       // todo
	//	m_cb.load_start();
	//	for (auto& n : m_new_nodes) {
	//		int tex_id = m_pages[n.page]->GetTexID();
	//		m_cb.load(tex_id, m_width, m_height, n.region, n.key);
	//	}
	//	m_cb.load_finish();

	//	dirty = true;
	//}

	m_new_nodes.clear();

//	if (bind_fbo) {
//		RenderAPI::GetRenderContext()->UnbindPixelBuffer();
//	}

	return dirty;
}

bool PixelBuffer::QueryAndInsert(uint64_t key, float* texcoords, ur2::TexturePtr& tex) const
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
//bool PixelBuffer::QueryRegion(uint64_t key, ur2::TexturePtr& tex, int& xmin, int& ymin, int& xmax, int& ymax) const
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


}