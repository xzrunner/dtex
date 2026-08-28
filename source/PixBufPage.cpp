#include "dtex/PixBufPage.h"
#include "dtex/TexPacker.h"

#include <unirender/Device.h>
#include <unirender/Context.h>
#include <unirender/TextureDescription.h>
#include <unirender/WritePixelBuffer.h>
#include <unirender/Texture.h>

#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{

const int MAX_NODE_SIZE = 512;

}

namespace dtex
{

PixBufPage::PixBufPage(const ur::Device& dev, size_t width, size_t height)
	: m_width(width)
	, m_height(height)
{
    const size_t max_buffer_bytes = static_cast<size_t>(std::numeric_limits<int>::max());
    if (width == 0 || height == 0 ||
        width > static_cast<size_t>(std::numeric_limits<int16_t>::max()) ||
        height > static_cast<size_t>(std::numeric_limits<int16_t>::max()) ||
        width > max_buffer_bytes / 4 ||
        height > max_buffer_bytes / (width * 4))
    {
        throw std::length_error("dtex pixel page dimensions are not representable");
    }
    const int page_width = static_cast<int>(width);
    const int page_height = static_cast<int>(height);
    const size_t buf_sz = width * height * 4;

    ur::TextureDescription desc;
    desc.target = ur::TextureTarget::Texture2D;
    desc.width  = page_width;
    desc.height = page_height;
    desc.format = ur::TextureFormat::RGBA8;
    m_tex = dev.CreateTexture(desc);

	m_tp = std::make_unique<TexPacker>(width, height, MAX_NODE_SIZE);

    m_pbuf = dev.CreateWritePixelBuffer(
        ur::BufferUsageHint::DynamicDraw, static_cast<int>(buf_sz));
    if (!m_pbuf) {
        // Metal: no PBO -> stage glyph pixels on the CPU and upload directly.
        m_cpu_buf.assign(buf_sz, 0);
    }

	InitDirtyRect();
}

Quad PixBufPage::AddToTP(size_t width, size_t height)
{
	return m_tp->Add(width, height, false);
}

void PixBufPage::Clear()
{
    const size_t buf_sz = m_width * m_height * 4;
    if (m_pbuf) {
        m_pbuf->ReadFromMemory(nullptr, static_cast<int>(buf_sz), 0);
    } else if (!m_cpu_buf.empty()) {
        memset(m_cpu_buf.data(), 0, m_cpu_buf.size());
    }

    m_tex->Upload(nullptr, 0, 0,
        static_cast<int>(m_width), static_cast<int>(m_height));

	m_tp->Clear();

	InitDirtyRect();
}

void PixBufPage::UpdateBitmap(ur::Context& ctx, const uint32_t* bitmap, int width,
                                    int height, const Rect& pos, const Rect& dirty_r)
{
	(void)ctx;
	if (!m_pbuf && m_cpu_buf.empty()) {
		return;
	}

#ifdef PBO_USE_MAP
	uint32_t* bmp_buf = reinterpret_cast<uint32_t*>(m_pbuf->Map());

    int src_ptr = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t src = bitmap[src_ptr++];
            uint8_t r = (src >> 24) & 0xff;
            uint8_t g = (src >> 16) & 0xff;
            uint8_t b = (src >> 8) & 0xff;
            uint8_t a = src & 0xff;

			const size_t dst_ptr =
				static_cast<size_t>(pos.ymin + y) * m_width +
				static_cast<size_t>(pos.xmin + x);
			bmp_buf[dst_ptr] = a << 24 | b << 16 | g << 8 | r;
        }
    }

#else
    // todo: update all, not row
    std::vector<uint32_t> line_buf(static_cast<size_t>(width));

	int src_ptr = 0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint32_t src = bitmap[src_ptr++];
			uint8_t r = (src >> 24) & 0xff;
			uint8_t g = (src >> 16) & 0xff;
			uint8_t b = (src >> 8) & 0xff;
			uint8_t a = src & 0xff;
            line_buf[static_cast<size_t>(x)] = a << 24 | b << 16 | g << 8 | r;
		}
        size_t off = ((pos.ymin + y) * m_width + pos.xmin) * 4;
        if (m_pbuf) {
            m_pbuf->ReadFromMemory(line_buf.data(), width * 4,
                static_cast<int>(off));
        } else {
            memcpy(m_cpu_buf.data() + off, line_buf.data(), width * 4);
        }
	}

    //ctx.SetUnpackRowLength(width);
    //m_pbuf->ReadFromMemory(line_buf, width * height * 4, (pos.ymin * m_width + pos.xmin) * 4);
    //ctx.SetUnpackRowLength(0);

#endif // PBO_USE_MAP

	UpdateDirtyRect(dirty_r);
}

bool PixBufPage::UploadTexture(ur::Context& ctx)
{
	if (m_dirty_rect.xmin >= m_dirty_rect.xmax ||
		m_dirty_rect.ymin >= m_dirty_rect.ymax) {
		return false;
	}

	int x = m_dirty_rect.xmin,
		y = m_dirty_rect.ymin;
	int w = m_dirty_rect.xmax - m_dirty_rect.xmin,
		h = m_dirty_rect.ymax - m_dirty_rect.ymin;

	if (!m_pbuf) {
		// Metal: no PBO. Upload the whole page from the CPU stage; its rows are
		// m_width wide so they are tight for a full-page replaceRegion. (A dirty
		// sub-rect would need a per-row copy because Texture::Upload assumes tight
		// rows; uploading the full page each new-glyph batch is simpler and cheap.)
		m_tex->Upload(m_cpu_buf.data(), 0, 0,
			static_cast<int>(m_width), static_cast<int>(m_height));
		InitDirtyRect();
		return true;
	}

    ctx.SetUnpackRowLength(static_cast<int>(m_width));
    const size_t offset_sz =
        (static_cast<size_t>(y) * m_width + static_cast<size_t>(x)) * 4;
    const int offset = static_cast<int>(offset_sz);

#ifdef PBO_USE_MAP
    m_pbuf->Unmap();
#else
    m_pbuf->Bind();
#endif // PBO_USE_MAP
    m_tex->Upload(reinterpret_cast<void*>(static_cast<std::intptr_t>(offset)), x, y, w, h);
	m_pbuf->UnBind();

    ctx.SetUnpackRowLength(0);

	InitDirtyRect();

	return true;
}

void PixBufPage::InitDirtyRect()
{
	m_dirty_rect.xmax = m_dirty_rect.ymax = 0;
	m_dirty_rect.xmin = static_cast<int16_t>(m_width);
	m_dirty_rect.ymin = static_cast<int16_t>(m_height);
}

void PixBufPage::UpdateDirtyRect(const Rect& r)
{
	if (r.xmin < m_dirty_rect.xmin) {
		m_dirty_rect.xmin = r.xmin;
	}
	if (r.ymin < m_dirty_rect.ymin) {
		m_dirty_rect.ymin = r.ymin;
	}
	if (r.xmax > m_dirty_rect.xmax) {
		m_dirty_rect.xmax = r.xmax;
	}
	if (r.ymax > m_dirty_rect.ymax) {
		m_dirty_rect.ymax = r.ymax;
	}
}

}
