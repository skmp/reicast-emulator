/*
	Copyright 2020 flyinghead

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
#include "vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta.h"
#include "commandpool.h"
#include "pipeline.h"
#include "vmu.h"

#include <memory>
#include <vector>

void co_dc_yield();

class BaseVulkanRenderer : public Renderer
{
public:
	virtual void Term() override
	{
		shaderManager.Term();
		vmus.reset();
		quadPipeline.Term();
		textureCache.Clear();
		fogTexture = nullptr;
		paletteTexture = nullptr;
		texCommandPool.Term();
		framebufferTextures.clear();
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
			// This kills performance when a frame is skipped and lots of texture updated each frame
			//if (textureCache.IsInFlight(tf))
			//	textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandPool.Allocate());
			tf->Update();
		}
		else if (tf->IsCustomTextureAvailable())
		{
			textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandPool.Allocate());
			tf->CheckCustomTexture();
		}
		tf->SetCommandBuffer(nullptr);
		textureCache.SetInFlight(tf);

		return tf->GetIntId();
	}

	virtual bool Process(TA_context* ctx) override
	{
		texCommandPool.BeginFrame();
		textureCache.SetCurrentIndex(texCommandPool.GetIndex());

		if (ctx->rend.isRenderFramebuffer)
			return RenderFramebuffer();

		ctx->rend_inuse.Lock();

		if (KillTex)
			textureCache.Clear();

		bool result = ta_parse_vdrc(ctx);

		textureCache.Cleanup();

		if (result)
		{
			CheckFogTexture();
			CheckPaletteTexture();
		}
		else
			texCommandPool.EndFrame();

		return result;
	}

	void Present() override
	{
		co_dc_yield();
	}

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	bool BaseInit(vk::RenderPass renderPass, int subpass = 0)
	{
		texCommandPool.Init();
		quadPipeline.Init(&shaderManager, renderPass, subpass);
		vmus = std::unique_ptr<VulkanOSD>(new VulkanOSD());
		vmus->Init(&quadPipeline);

		return true;
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
		curTexture->UploadToGPU(width, height, (u8*)pb.data(), false);
		curTexture->SetCommandBuffer(nullptr);
		texCommandPool.EndFrame();

		GetContext()->PresentFrame(curTexture->GetImage(), curTexture->GetImageView(), { 640, 480 });

		return true;
	}

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
		if (!fog_needs_update || !settings.rend.Fog)
			return;
		fog_needs_update = false;
		u8 texData[256];
		MakeFogTexture(texData);
		fogTexture->SetCommandBuffer(texCommandPool.Allocate());

		fogTexture->UploadToGPU(128, 2, texData, false);

		fogTexture->SetCommandBuffer(nullptr);
	}

	void CheckPaletteTexture()
	{
		if (!paletteTexture)
		{
			paletteTexture = std::unique_ptr<Texture>(new Texture());
			paletteTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			paletteTexture->SetDevice(GetContext()->GetDevice());
			paletteTexture->tex_type = TextureType::_8888;
			palette_updated = true;
		}
		if (!palette_updated)
			return;
		palette_updated = false;

		paletteTexture->SetCommandBuffer(texCommandPool.Allocate());

		paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);

		paletteTexture->SetCommandBuffer(nullptr);
	}

	ShaderManager shaderManager;
	std::unique_ptr<Texture> fogTexture;
	std::unique_ptr<Texture> paletteTexture;
	CommandPool texCommandPool;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	TextureCache textureCache;
	QuadPipeline quadPipeline;
	std::unique_ptr<VulkanOSD> vmus;
};

