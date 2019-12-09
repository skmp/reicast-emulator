/*
    Created on: Nov 7, 2019

	Copyright 2019 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <memory>
#include <math.h>
#include "../vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "../commandpool.h"
#include "oit_drawer.h"
#include "oit_shaders.h"
#include "../pipeline.h"
#include "oit_buffer.h"

void co_dc_yield();

class OITVulkanRenderer : public Renderer
{
public:
	bool Init() override
	{
		DEBUG_LOG(RENDERER, "OITVulkanRenderer::Init");
		texCommandPool.Init();

		oitBuffers.Init(0, 0);
		textureDrawer.Init(&samplerManager, &shaderManager, &textureCache, &oitBuffers);
		textureDrawer.SetCommandPool(&texCommandPool);

		screenDrawer.Init(&samplerManager, &shaderManager, &oitBuffers);
		screenDrawer.SetCommandPool(&texCommandPool);
		quadBuffer = std::unique_ptr<QuadBuffer>(new QuadBuffer());

		return true;
	}

	void Resize(int w, int h) override
	{
		NOTICE_LOG(RENDERER, "OIT Resize %d x %d", w, h);
		texCommandPool.Init();
		screenDrawer.Init(&samplerManager, &shaderManager, &oitBuffers);
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Term");
		GetContext()->WaitIdle();
		screenDrawer.Term();
		textureDrawer.Term();
		oitBuffers.Term();
		quadBuffer = nullptr;
		textureCache.Clear();
		fogTexture = nullptr;
		texCommandPool.Term();
		framebufferTextures.clear();
	}

	bool RenderFramebuffer()
	{
		if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
			return false;

		PixelBuffer<u32> pb;
		int width;
		int height;
		ReadFramebuffer(pb, width, height);

		if (framebufferTextures.size() != GetContext()->GetSwapChainSize())
			framebufferTextures.resize(GetContext()->GetSwapChainSize());
		std::unique_ptr<Texture>& curTexture = framebufferTextures[GetContext()->GetCurrentImageIndex()];
		if (!curTexture)
		{
			curTexture = std::unique_ptr<Texture>(new Texture());
			curTexture->tex_type = TextureType::_8888;
			curTexture->tcw.full = 0;
			curTexture->tsp.full = 0;
			curTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			curTexture->SetDevice(GetContext()->GetDevice());
		}
		curTexture->SetCommandBuffer(texCommandPool.Allocate());
		curTexture->UploadToGPU(width, height, (u8*)pb.data());
		curTexture->SetCommandBuffer(nullptr);
		texCommandPool.EndFrame();

		GetContext()->PresentFrame(curTexture->GetImage(), curTexture->GetImageView(), { 640, 480 });

		return true;
	}

	bool Process(TA_context* ctx) override
	{
		texCommandPool.BeginFrame();
		textureCache.SetCurrentIndex(texCommandPool.GetIndex());

		if (ctx->rend.isRenderFramebuffer)
		{
			return RenderFramebuffer();
		}

		ctx->rend_inuse.Lock();

		if (KillTex)
			textureCache.Clear();

		bool result = ta_parse_vdrc(ctx);

		textureCache.CollectCleanup();

		if (ctx->rend.Overrun)
			WARN_LOG(PVR, "ERROR: TA context overrun");

		result = result && !ctx->rend.Overrun;

		if (result)
			CheckFogTexture();

		if (!result)
			texCommandPool.EndFrame();

		return result;
	}

	bool Render() override
	{
		if (pvrrc.isRenderFramebuffer)
			return true;

		OITDrawer *drawer;
		if (pvrrc.isRTT)
			drawer = &textureDrawer;
		else
			drawer = &screenDrawer;

		drawer->Draw(fogTexture.get());

		drawer->EndFrame();

		return !pvrrc.isRTT;
	}

	void Present() override
	{
		co_dc_yield();
	}

	virtual u64 GetTexture(TSP tsp, TCW tcw) override
	{
		Texture* tf = textureCache.getTextureCacheData(tsp, tcw);

		if (tf->IsNew())
		{
			tf->Create();
			tf->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			tf->SetDevice(GetContext()->GetDevice());
		}

		//update if needed
		if (tf->NeedsUpdate())
		{
			textureCache.DestroyLater(tf);

			tf->SetCommandBuffer(texCommandPool.Allocate());
			tf->Update();
			tf->SetCommandBuffer(nullptr);
		}
		else
			tf->CheckCustomTexture();

		return tf->GetIntId();
	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	void CheckFogTexture()
	{
		if (!fogTexture)
		{
			fogTexture = std::unique_ptr<Texture>(new Texture());
			fogTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			fogTexture->SetDevice(GetContext()->GetDevice());
			fogTexture->tex_type = TextureType::_8;
			fog_needs_update = true;
		}
		if (!fog_needs_update)
			return;
		fog_needs_update = false;
		u8 texData[256];
		MakeFogTexture(texData);
		fogTexture->SetCommandBuffer(texCommandPool.Allocate());

		fogTexture->UploadToGPU(128, 2, texData);

		fogTexture->SetCommandBuffer(nullptr);
	}

	OITBuffers oitBuffers;
	std::unique_ptr<QuadBuffer> quadBuffer;
	std::unique_ptr<Texture> fogTexture;
	CommandPool texCommandPool;

	SamplerManager samplerManager;
	OITShaderManager shaderManager;
	ShaderManager normalShaderManager;
	OITScreenDrawer screenDrawer;
	OITTextureDrawer textureDrawer;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	TextureCache textureCache;
};

Renderer* rend_OITVulkan()
{
	return new OITVulkanRenderer();
}
