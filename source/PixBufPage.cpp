#include "dtex/PixBufPage.h"
#include "dtex/TexPacker.h"

#include <unirender/Device.h>
#include <unirender/Context.h>
#include <unirender/TextureDescription.h>
#include <unirender/WritePixelBuffer.h>
#include <unirender/Texture.h>

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
    ur::TextureDescription desc;
    desc.target = ur::TextureTarget::Texture2D;
    desc.width  = width;
    desc.height = height;
    desc.format = ur::TextureFormat::RGBA8;
    m_tex = dev.CreateTexture(desc);

	m_tp = std::make_unique<TexPacker>(width, height, MAX_NODE_SIZE);

    auto buf_sz = width * height * 4;
    m_pbuf = dev.CreateWritePixelBuffer(ur::BufferUsageHint::DynamicDraw, buf_sz);

	InitDirtyRect();
}

Quad PixBufPage::AddToTP(size_t width, size_t height)
{
	return m_tp->Add(width, height, false);
}

void PixBufPage::Clear()
{
    auto buf_sz = m_width * m_height * 4;
    m_pbuf->ReadFromMemory(nullptr, buf_sz, 0);

    m_tex->Upload(nullptr, 0, 0, m_width, m_height);

	m_tp->Clear();

	InitDirtyRect();
}

void PixBufPage::UpdateBitmap(ur::Context& ctx, const uint32_t* bitmap, int width,
                                    int height, const Rect& pos, const Rect& dirty_r)
{
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

            int dst_ptr = (pos.ymin + y) * m_width + pos.xmin + x;
            bmp_buf[dst_ptr] = a << 24 | b << 16 | g << 8 | r;
        }
    }

#else
    // todo: update all, not row
    uint32_t* line_buf = new uint32_t[width];

	int src_ptr = 0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint32_t src = bitmap[src_ptr++];
			uint8_t r = (src >> 24) & 0xff;
			uint8_t g = (src >> 16) & 0xff;
			uint8_t b = (src >> 8) & 0xff;
			uint8_t a = src & 0xff;
			line_buf[x] = a << 24 | b << 16 | g << 8 | r;
		}
        m_pbuf->ReadFromMemory(line_buf, width * 4, ((pos.ymin + y) * m_width + pos.xmin) * 4);
	}

    //ctx.SetUnpackRowLength(width);
    //m_pbuf->ReadFromMemory(line_buf, width * height * 4, (pos.ymin * m_width + pos.xmin) * 4);
    //ctx.SetUnpackRowLength(0);

    delete[] line_buf;
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
	ctx.SetUnpackRowLength(m_width);
	int offset = (y * m_width + x) * 4;

#ifdef PBO_USE_MAP
    m_pbuf->Unmap();
#else
    m_pbuf->Bind();
#endif // PBO_USE_MAP
    m_tex->Upload(reinterpret_cast<void*>(offset), x, y, w, h);

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