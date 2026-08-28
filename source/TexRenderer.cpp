#include "dtex/TexRenderer.h"

#include <unirender/Device.h>
#include <unirender/Context.h>
#include <unirender/Texture.h>
#include <unirender/ClearState.h>
#include <unirender/Framebuffer.h>
#include <unirender/DrawState.h>
#include <unirender/Factory.h>
#include <unirender/VertexArray.h>
#include <unirender/IndexBuffer.h>
#include <unirender/VertexBuffer.h>
#include <unirender/ComponentDataType.h>
#include <unirender/VertexInputAttribute.h>
#include <shadertrans/ShaderTrans.h>

#include <cstdint>
#include <cmath>
#include <limits>
#include <memory>

namespace
{

struct LogicalTargetGuard
{
    ur::Context& ctx;
    std::shared_ptr<ur::Framebuffer> framebuffer;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    explicit LogicalTargetGuard(ur::Context& context)
        : ctx(context)
        , framebuffer(context.GetFramebuffer())
    {
        ctx.GetViewport(x, y, w, h);
    }

    ~LogicalTargetGuard()
    {
        ctx.SetFramebuffer(framebuffer);
        ctx.SetViewport(x, y, w, h);
        ctx.CommitFramebuffer();
        ctx.CommitViewport();
    }
};

const char* vs = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
	TexCoord = vec2(aTexCoord.x, aTexCoord.y);
}
)";

const char* fs = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;

void main()
{
	FragColor = texture(texture1, TexCoord);
}
)";

}

namespace dtex
{

TexRenderer::TexRenderer(const ur::Device& dev)
{
    std::vector<unsigned int> _vs, _fs;
    shadertrans::ShaderTrans::GLSL2SpirV(shadertrans::ShaderStage::VertexShader, vs, nullptr, _vs);
    shadertrans::ShaderTrans::GLSL2SpirV(shadertrans::ShaderStage::PixelShader, fs, nullptr, _fs);
    m_shader = dev.CreateShaderProgram(_vs, _fs);

    m_rt = dev.CreateFramebuffer();

    InitVertexArray(dev);
}

bool TexRenderer::Draw(ur::Context& ctx, const ur::TexturePtr& src, const Rect& src_r,
                       const ur::TexturePtr& dst, const Rect& dst_r, bool rotate)
{
    if (!src || !dst || src->GetWidth() <= 0 || src->GetHeight() <= 0 ||
        dst->GetWidth() <= 0 || dst->GetHeight() <= 0 ||
        src_r.xmin < 0 || src_r.ymin < 0 ||
        src_r.xmax <= src_r.xmin || src_r.ymax <= src_r.ymin ||
        src_r.xmax > src->GetWidth() || src_r.ymax > src->GetHeight() ||
        dst_r.xmin < 0 || dst_r.ymin < 0 ||
        dst_r.xmax <= dst_r.xmin || dst_r.ymax <= dst_r.ymin ||
        dst_r.xmax > dst->GetWidth() || dst_r.ymax > dst->GetHeight())
    {
        return false;
    }
    if (dst != m_dst_texture || src != m_src_texture)
    {
        if (!Flush(ctx))
        {
            return false;
        }

        m_dst_texture = dst;
        m_src_texture = src;
    }

	float vertices[8];
	float w_inv = 1.0f / dst->GetWidth(),
		  h_inv = 1.0f / dst->GetHeight();
	float dst_xmin = dst_r.xmin * w_inv * 2 - 1,
		  dst_xmax = dst_r.xmax * w_inv * 2 - 1,
		  dst_ymin = dst_r.ymin * h_inv * 2 - 1,
		  dst_ymax = dst_r.ymax * h_inv * 2 - 1;
	vertices[0] = dst_xmin; vertices[1] = dst_ymin;
	vertices[2] = dst_xmax; vertices[3] = dst_ymin;
	vertices[4] = dst_xmax; vertices[5] = dst_ymax;
	vertices[6] = dst_xmin; vertices[7] = dst_ymax;
	if (rotate)
	{
		float x, y;
		x = vertices[6]; y = vertices[7];
		vertices[6] = vertices[4]; vertices[7] = vertices[5];
		vertices[4] = vertices[2]; vertices[5] = vertices[3];
		vertices[2] = vertices[0]; vertices[3] = vertices[1];
		vertices[0] = x;           vertices[1] = y;
	}

	float texcoords[8];
	float src_w_inv = 1.0f / src->GetWidth(),
		  src_h_inv = 1.0f / src->GetHeight();
	float src_xmin = src_r.xmin * src_w_inv,
		  src_xmax = src_r.xmax * src_w_inv,
		  src_ymin = src_r.ymin * src_h_inv,
		  src_ymax = src_r.ymax * src_h_inv;
	texcoords[0] = src_xmin; texcoords[1] = src_ymin;
	texcoords[2] = src_xmax; texcoords[3] = src_ymin;
	texcoords[4] = src_xmax; texcoords[5] = src_ymax;
	texcoords[6] = src_xmin; texcoords[7] = src_ymax;

    return m_vert_buf.AddQuad(vertices, texcoords);
}

void TexRenderer::DiscardPending()
{
    m_vert_buf.Clear();
    m_src_texture = nullptr;
    m_dst_texture = nullptr;
}

#ifdef DTEX_ENABLE_TEST_SEAMS
void TexRenderer::FailNextFlush(int count)
{
    m_fail_next_flush = count > 0 ? count : 0;
}

bool TexRenderer::ConsumeFailNextFlush()
{
    if (m_fail_next_flush > 0) {
        --m_fail_next_flush;
        return false;
    }
    return true;
}

void TexRenderer::FailNextGpuFlushForTest(int count)
{
    m_fail_next_gpu_flush = count > 0 ? count : 0;
}
#endif

bool TexRenderer::HasPending() const
{
    return !m_vert_buf.IsEmpty();
}

bool TexRenderer::Flush(ur::Context& ctx)
{
#ifdef DTEX_ENABLE_TEST_SEAMS
    if (m_fail_next_gpu_flush > 0) {
        --m_fail_next_gpu_flush;
        return false;
    }
    if (m_fail_next_flush > 0) {
        --m_fail_next_flush;
        return false;
    }
#endif
    if (m_vert_buf.IsEmpty()) {
        return true;
    }

    LogicalTargetGuard guard(ctx);

    const auto type = ur::AttachmentType::Color0;
    m_rt->SetAttachment(type, ur::TextureTarget::Texture2D, m_dst_texture, nullptr);

    ctx.SetFramebuffer(m_rt);
    if (!ctx.CheckRenderTargetStatus()) {
        return false;
    }

    if (m_vert_buf.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max()) / sizeof(std::uint32_t) ||
        m_vert_buf.vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max()) / sizeof(Vertex) ||
        m_vert_buf.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }
    const size_t ibuf_bytes = sizeof(std::uint32_t) * m_vert_buf.indices.size();
    const size_t vbuf_bytes = sizeof(Vertex) * m_vert_buf.vertices.size();
    const int ibuf_sz = static_cast<int>(ibuf_bytes);
    auto ibuf = m_va->GetIndexBuffer();
    ibuf->SetCount(static_cast<int>(m_vert_buf.indices.size()));
    ibuf->Reserve(ibuf_sz);
    ibuf->ReadFromMemory(m_vert_buf.indices.data(), ibuf_sz, 0);
    m_va->SetIndexBuffer(ibuf);

    const int vbuf_sz = static_cast<int>(vbuf_bytes);
    auto vbuf = m_va->GetVertexBuffer();
    vbuf->Reserve(vbuf_sz);
    vbuf->ReadFromMemory(m_vert_buf.vertices.data(), vbuf_sz, 0);
    m_va->SetVertexBuffer(vbuf);

    m_va->SetVertexBufferAttrs({
        std::make_shared<ur::VertexInputAttribute>(0, ur::ComponentDataType::Float, 2, 0, 16),
        std::make_shared<ur::VertexInputAttribute>(1, ur::ComponentDataType::Float, 2, 8, 16)
    });

    ctx.SetViewport(0, 0, m_dst_texture->GetWidth(), m_dst_texture->GetHeight());

    ctx.SetTexture(0, m_src_texture);
    ctx.SetTextureSampler(0, nullptr);

    auto rs = ur::DefaultRenderState2D();
    rs.blending.src = ur::BlendingFactor::One;

    ur::DrawState ds;
    ds.program      = m_shader;
    ds.render_state = rs;
    ds.vertex_array = m_va;
    ctx.Draw(ur::PrimitiveType::Triangles, ds, nullptr);
    m_vert_buf.Clear();
    return true;
}

bool TexRenderer::ClearTex(ur::Context& ctx, const ur::TexturePtr& tex,
                           float xmin, float ymin, float xmax, float ymax) const
{
    if (!tex || tex->GetWidth() <= 0 || tex->GetHeight() <= 0 ||
        !std::isfinite(xmin) || !std::isfinite(ymin) ||
        !std::isfinite(xmax) || !std::isfinite(ymax) ||
        xmin < 0.0f || ymin < 0.0f || xmax > 1.0f || ymax > 1.0f ||
        xmax <= xmin || ymax <= ymin)
    {
        return false;
    }

    LogicalTargetGuard guard(ctx);

    const auto type = ur::AttachmentType::Color0;
    m_rt->SetAttachment(type, ur::TextureTarget::Texture2D, tex, nullptr);
    ctx.SetFramebuffer(m_rt);
    if (!ctx.CheckRenderTargetStatus()) {
        return false;
    }

    ur::ClearState cs;

    cs.scissor_test.enabled = true;
	int w = tex->GetWidth(),
		h = tex->GetHeight();
    cs.scissor_test.rect.x = static_cast<int>(w * xmin);
    cs.scissor_test.rect.y = static_cast<int>(h * ymin);
    cs.scissor_test.rect.w = static_cast<int>(w * (xmax - xmin));
    cs.scissor_test.rect.h = static_cast<int>(h * (ymax - ymin));

    cs.color.FromRGBA(0);

    ctx.Clear(cs);
    return true;
}

bool TexRenderer::ClearAllTex(ur::Context& ctx, const ur::TexturePtr& tex) const
{
    if (!tex || tex->GetWidth() <= 0 || tex->GetHeight() <= 0)
    {
        return false;
    }

    LogicalTargetGuard guard(ctx);

    const auto type = ur::AttachmentType::Color0;
    m_rt->SetAttachment(type, ur::TextureTarget::Texture2D, tex, nullptr);
    ctx.SetFramebuffer(m_rt);
    if (!ctx.CheckRenderTargetStatus()) {
        return false;
    }

    ur::ClearState cs;
    cs.color.FromRGBA(0);

    ctx.Clear(cs);
    return true;
}

void TexRenderer::InitVertexArray(const ur::Device& dev)
{
    m_va = dev.CreateVertexArray();

    auto usage = ur::BufferUsageHint::StaticDraw;

    auto ibuf = dev.CreateIndexBuffer(usage, 0);
    ibuf->SetDataType(ur::IndexBufferDataType::UnsignedInt);
    m_va->SetIndexBuffer(ibuf);

    auto vbuf = dev.CreateVertexBuffer(ur::BufferUsageHint::StaticDraw, 0);
    m_va->SetVertexBuffer(vbuf);
}

//////////////////////////////////////////////////////////////////////////
// class TexRenderer::VertBuffer
//////////////////////////////////////////////////////////////////////////

bool TexRenderer::VertBuffer::
AddQuad(const float* positions, const float* texcoords)
{
    const size_t old_vertices = vertices.size();
    const size_t old_indices = indices.size();
    if (old_vertices > static_cast<size_t>(std::numeric_limits<std::uint32_t>::max()) - 4 ||
        old_indices > std::numeric_limits<size_t>::max() - 6) {
        return false;
    }
    try {
        vertices.resize(old_vertices + 4);
        indices.resize(old_indices + 6);
    } catch (...) {
        vertices.resize(old_vertices);
        indices.resize(old_indices);
        return false;
    }

    const auto base = static_cast<std::uint32_t>(old_vertices);
    indices[old_indices + 0] = base;
    indices[old_indices + 1] = base + 1;
    indices[old_indices + 2] = base + 2;
    indices[old_indices + 3] = base;
    indices[old_indices + 4] = base + 2;
    indices[old_indices + 5] = base + 3;

    int ptr = 0;
    for (int i = 0; i < 4; ++i)
    {
        auto& v = vertices[old_vertices + static_cast<size_t>(i)];
        v.pos[0] = positions[ptr];
        v.pos[1] = positions[ptr + 1];
        v.uv[0]  = texcoords[ptr];
        v.uv[1]  = texcoords[ptr + 1];
        ptr += 2;
    }
    return true;
}

void TexRenderer::VertBuffer::
Clear()
{
    vertices.resize(0);
    indices.resize(0);
}

}
