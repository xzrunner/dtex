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

namespace
{

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
    shadertrans::ShaderTrans::GLSL2SpirV(shadertrans::ShaderStage::VertexShader, vs, _vs);
    shadertrans::ShaderTrans::GLSL2SpirV(shadertrans::ShaderStage::PixelShader, fs, _fs);
    m_shader = dev.CreateShaderProgram(_vs, _fs);

    m_rt = dev.CreateFramebuffer();

    InitVertexArray(dev);
}

void TexRenderer::Draw(ur::Context& ctx, const ur::TexturePtr& src, const Rect& src_r,
                       const ur::TexturePtr& dst, const Rect& dst_r, bool rotate)
{
    if (dst != m_dst_texture || src != m_src_texture)
    {
        Flush(ctx);

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

    m_vert_buf.AddQuad(vertices, texcoords);
}

void TexRenderer::Flush(ur::Context& ctx)
{
    if (m_vert_buf.IsEmpty()) {
        return;
    }

    const auto type = ur::AttachmentType::Color0;
    m_rt->SetAttachment(type, ur::TextureTarget::Texture2D, m_dst_texture, nullptr);

    auto ibuf_sz = sizeof(unsigned short) * m_vert_buf.indices.size();
    auto ibuf = m_va->GetIndexBuffer();
    ibuf->SetCount(m_vert_buf.indices.size());
    ibuf->Reserve(ibuf_sz);
    ibuf->ReadFromMemory(m_vert_buf.indices.data(), ibuf_sz, 0);
    m_va->SetIndexBuffer(ibuf);

    auto vbuf_sz = sizeof(Vertex) * m_vert_buf.vertices.size();
    auto vbuf = m_va->GetVertexBuffer();
    vbuf->Reserve(vbuf_sz);
    vbuf->ReadFromMemory(m_vert_buf.vertices.data(), vbuf_sz, 0);
    m_va->SetVertexBuffer(vbuf);

    m_vert_buf.Clear();

    m_va->SetVertexBufferAttrs({
        std::make_shared<ur::VertexInputAttribute>(0, ur::ComponentDataType::Float, 2, 0, 16),
        std::make_shared<ur::VertexInputAttribute>(1, ur::ComponentDataType::Float, 2, 8, 16)
    });

    int x, y, w, h;
    ctx.GetViewport(x, y, w, h);

    ctx.SetFramebuffer(m_rt);
    ctx.SetViewport(0, 0, m_dst_texture->GetWidth(), m_dst_texture->GetHeight());

    ctx.SetTexture(0, m_src_texture);

    auto rs = ur::DefaultRenderState2D();
    rs.blending.src = ur::BlendingFactor::One;

    ur::DrawState ds;
    ds.program      = m_shader;
    ds.render_state = rs;
    ds.vertex_array = m_va;
    ctx.Draw(ur::PrimitiveType::Triangles, ds, nullptr);

    ctx.SetFramebuffer(nullptr);
    ctx.SetViewport(x, y, w, h);
}

void TexRenderer::ClearTex(ur::Context& ctx, const ur::TexturePtr& tex,
                           float xmin, float ymin, float xmax, float ymax) const
{
    const auto type = ur::AttachmentType::Color0;
    m_rt->SetAttachment(type, ur::TextureTarget::Texture2D, tex, nullptr);
    if (!ctx.CheckRenderTargetStatus()) {
        return;
    }

    ctx.SetFramebuffer(m_rt);

    ur::ClearState cs;

    cs.scissor_test.enabled = true;
	int w = tex->GetWidth(),
		h = tex->GetHeight();
    cs.scissor_test.rect.x = static_cast<int>(w * xmin);
    cs.scissor_test.rect.y = static_cast<int>(h * xmin);
    cs.scissor_test.rect.w = static_cast<int>(w * (xmax - xmin));
    cs.scissor_test.rect.h = static_cast<int>(h * (ymax - ymin));

    cs.color.FromRGBA(0);

    ctx.Clear(cs);
}

void TexRenderer::ClearAllTex(ur::Context& ctx, const ur::TexturePtr& tex) const
{
    const auto type = ur::AttachmentType::Color0;
    m_rt->SetAttachment(type, ur::TextureTarget::Texture2D, tex, nullptr);
    if (!ctx.CheckRenderTargetStatus()) {
        return;
    }

    ctx.SetFramebuffer(m_rt);

    ur::ClearState cs;
    cs.color.FromRGBA(0);

    ctx.Clear(cs);
}

void TexRenderer::InitVertexArray(const ur::Device& dev)
{
    m_va = dev.CreateVertexArray();

    auto usage = ur::BufferUsageHint::StaticDraw;

    auto ibuf = dev.CreateIndexBuffer(usage, 0);
    m_va->SetIndexBuffer(ibuf);

    auto vbuf = dev.CreateVertexBuffer(ur::BufferUsageHint::StaticDraw, 0);
    m_va->SetVertexBuffer(vbuf);
}

//////////////////////////////////////////////////////////////////////////
// class TexRenderer::VertBuffer
//////////////////////////////////////////////////////////////////////////

void TexRenderer::VertBuffer::
AddQuad(const float* positions, const float* texcoords)
{
    Reserve(6, 4);

    index_ptr[0] = curr_index;
    index_ptr[1] = curr_index + 1;
    index_ptr[2] = curr_index + 2;
    index_ptr[3] = curr_index;
    index_ptr[4] = curr_index + 2;
    index_ptr[5] = curr_index + 3;
    index_ptr += 6;

    int ptr = 0;
    for (int i = 0; i < 4; ++i)
    {
        auto& v = vert_ptr[i];
        v.pos[0] = positions[ptr];
        v.pos[1] = positions[ptr + 1];
        v.uv[0]  = texcoords[ptr];
        v.uv[1]  = texcoords[ptr + 1];
        ptr += 2;
    }
    vert_ptr += 4;

    curr_index += 4;
}

void TexRenderer::VertBuffer::
Reserve(size_t idx_count, size_t vtx_count)
{
    size_t sz = vertices.size();
    vertices.resize(sz + vtx_count);
    vert_ptr = vertices.data() + sz;

    sz = indices.size();
    indices.resize(sz + idx_count);
    index_ptr = indices.data() + sz;
}

void TexRenderer::VertBuffer::
Clear()
{
    vertices.resize(0);
    indices.resize(0);

    curr_index = 0;
    vert_ptr = nullptr;
    index_ptr = nullptr;
}

}