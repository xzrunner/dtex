#include "dtex/PixelBuffer.h"
#include "dtex/PixBufPage.h"
#include "dtex/TextureBuffer.h"
#include "dtex/TexRenderer.h"

#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>

namespace
{

const int PADDING = 1;

class LoadDepthGuard
{
public:
	explicit LoadDepthGuard(dtex::TextureBuffer& buffer) noexcept
		: m_buffer(buffer) {}

	~LoadDepthGuard()
	{
		if (m_active) {
			m_buffer.AbortLoad();
		}
	}

	void release() noexcept { m_active = false; }

private:
	dtex::TextureBuffer& m_buffer;
	bool m_active = true;
};

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
	const int64_t pw64 = static_cast<int64_t>(width) + PADDING * 2;
	const int64_t ph64 = static_cast<int64_t>(height) + PADDING * 2;
	if (!bitmap || width <= 0 || height <= 0 || pw64 <= 0 || ph64 <= 0 ||
		pw64 > std::numeric_limits<int>::max() ||
		ph64 > std::numeric_limits<int>::max() ||
	    (!(pw64 <= m_width && ph64 <= m_height) &&
		 !(ph64 <= m_width && pw64 <= m_height))) {
		return;
	}
	const int pw = static_cast<int>(pw64);
	const int ph = static_cast<int>(ph64);
	if (Exist(key)) {
		return;
	}

	// Allocate the two logical-index containers before the packer is mutated.
	// After a successful map insertion, the pending-vector push is guaranteed
	// not to allocate and Node's copy cannot throw, so a key can never become
	// Exist() without also being queued for its first atlas publication.
	static_assert(std::is_nothrow_copy_constructible<Node>::value,
		"PixelBuffer::Node must support a no-throw logical commit");
	try {
#ifdef DTEX_ENABLE_TEST_SEAMS
		if (m_fail_next_load_prepare > 0) {
			--m_fail_next_load_prepare;
			throw std::bad_alloc();
		}
#endif
		if (m_new_nodes.size() == std::numeric_limits<size_t>::max() ||
			m_all_nodes.size() == std::numeric_limits<size_t>::max()) {
			return;
		}
		m_new_nodes.reserve(m_new_nodes.size() + 1);
		m_all_nodes.reserve(m_all_nodes.size() + 1);
	} catch (...) {
		return;
	}

	try {
		Quad dst_pos;

		size_t page_idx = m_pages.size();
		for (size_t i = 0, n = m_pages.size(); i < n; ++i)
		{
			dst_pos = m_pages[i]->AddToTP(pw, ph);
			if (dst_pos.rect.IsValid()) {
				page_idx = i;
				break;
			}
		}
		if (page_idx == m_pages.size())
		{
			auto page = std::make_unique<PixBufPage>(dev, m_width, m_height);
			dst_pos = page->AddToTP(pw, ph);
			if (!dst_pos.rect.IsValid()) {
				return;
			}
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
		m_pages[page_idx]->UpdateBitmap(ctx, bitmap, width, height,
			r_no_padding, dst_pos.rect);

		const auto inserted = m_all_nodes.insert({ key, node });
		if (!inserted.second) {
			return;
		}
		m_new_nodes.push_back(node);
	} catch (...) {
		// Packer/bitmap holes are harmless and reusable space is only lost for
		// this page. The key is not committed unless its first pending entry is
		// guaranteed, so callers may safely retry the same key.
		return;
	}
}

PixelBuffer::FlushResult PixelBuffer::Flush(ur::Context& ctx, TextureBuffer& tex_buf, TexRenderer& rd)
{
	FlushResult result;

	if (m_new_nodes.empty()) {
		if (rd.HasPending()) {
			result.success = rd.Flush(ctx);
			result.had_work = true;
			return result;
		}
		return result;
	}

	result.had_work = true;
	try {
		for (auto& p : m_pages) {
			p->UploadTexture(ctx);
		}

		if (!tex_buf.LoadStart()) {
			result.success = false;
			return result;
		}
		LoadDepthGuard depth_guard(tex_buf);
		for (auto& n : m_new_nodes) {
			auto tex = m_pages[n.page]->GetTexture();
			(void)tex_buf.Load(tex, n.region, n.key, 1, 0);
		}
		const bool finished = tex_buf.LoadFinish(ctx, rd);
		depth_guard.release();
		if (!finished) {
			result.success = false;
			return result;
		}

		result.success = true;
		for (auto itr = m_new_nodes.begin(); itr != m_new_nodes.end(); ) {
			int block_id = -1;
			if (tex_buf.Query(itr->key, block_id)) {
				itr = m_new_nodes.erase(itr);
			} else {
				result.success = false;
				++itr;
			}
		}
		return result;
	} catch (...) {
		result.success = false;
		return result;
	}
}

bool PixelBuffer::QueryAndInsert(uint64_t key, float* texcoords, ur::TexturePtr& tex) const
{
	tex = nullptr;
	if (!texcoords) {
		return false;
	}
	auto itr = m_all_nodes.find(key);
	if (itr == m_all_nodes.end()) {
		return false;
	}

	auto& node = itr->second;
	if (node.page >= m_pages.size() || !m_pages[node.page]) {
		return false;
	}
	try {
#ifdef DTEX_ENABLE_TEST_SEAMS
		if (m_fail_next_query_queue > 0) {
			--m_fail_next_query_queue;
			throw std::bad_alloc();
		}
#endif
		m_new_nodes.push_back(node);
	} catch (...) {
		return false;
	}

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

#ifdef DTEX_ENABLE_TEST_SEAMS
void PixelBuffer::FailNextLoadPrepareForTest(int count) noexcept
{
	m_fail_next_load_prepare = count > 0 ? count : 0;
}

void PixelBuffer::FailNextQueryQueueForTest(int count) noexcept
{
	m_fail_next_query_queue = count > 0 ? count : 0;
}
#endif

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
