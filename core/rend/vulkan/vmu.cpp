/*
    Created on: Dec 13, 2019

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
#include "vmu.h"
#include "texture.h"

VulkanOSD::~VulkanOSD()
{
}

const std::vector<vk::CommandBuffer>* VulkanOSD::PrepareOSD(CommandPool *commandPool)
{
	VulkanContext *context = VulkanContext::Instance();
	commandBuffers.resize(context->GetSwapChainSize());
	commandBuffers[context->GetCurrentImageIndex()].clear();
	for (int i = 0; i < VMU_COUNT; i++)
	{
		std::unique_ptr<Texture>& texture = vmuTextures[i];
		auto& vmu_screen_param = vmu_screen_params[i];

		if (!vmu_screen_param.vmu_screen_display || vmu_screen_param.vmu_lcd_screen == nullptr)
		{
			texture.reset();
			continue;
		}
		if (!texture)
			texture = std::unique_ptr<Texture>(new Texture());
		else if (!vmu_screen_param.vmu_screen_needs_update)
			continue;

		u8 temp_tex_buffer[VMU_SCREEN_HEIGHT * VMU_SCREEN_WIDTH * 4];
		u8 *dst = temp_tex_buffer;
		u8 *src = vmu_screen_param.vmu_lcd_screen;

		for (int y = 0; y < VMU_SCREEN_HEIGHT; y++)
		{
			for (int x = 0; x < VMU_SCREEN_WIDTH; x++)
			{
				if (*src++)
				{
					*dst++ = vmu_screen_param.vmu_pixel_on_R;
					*dst++ = vmu_screen_param.vmu_pixel_on_G;
					*dst++ = vmu_screen_param.vmu_pixel_on_B;
				}
				else
				{
					*dst++ = vmu_screen_param.vmu_pixel_off_R;
					*dst++ = vmu_screen_param.vmu_pixel_off_G;
					*dst++ = vmu_screen_param.vmu_pixel_off_B;
				}
				*dst++ = 0xff;
			}
		}

		texture->tex_type = TextureType::_8888;
		texture->SetDevice(context->GetDevice());
		texture->SetPhysicalDevice(context->GetPhysicalDevice());
		commandBuffers[context->GetCurrentImageIndex()].push_back(commandPool->Allocate());
		texture->SetCommandBuffer(commandBuffers[context->GetCurrentImageIndex()].back());
		texture->UploadToGPU(VMU_SCREEN_WIDTH, VMU_SCREEN_HEIGHT, temp_tex_buffer, false);
		texture->SetCommandBuffer(nullptr);
		vmu_screen_param.vmu_screen_needs_update = false;
	}

	for (int i = 0; i < XHAIR_COUNT; i++)
	{
		std::unique_ptr<Texture>& texture = xhairTextures[i];
		if (lightgun_params[i].offscreen || lightgun_params[i].colour == 0)
		{
			texture.reset();
			continue;
		}
		if (!texture)
			texture = std::unique_ptr<Texture>(new Texture());
		else if (!lightgun_params[i].dirty)
			continue;

		u8 temp_tex_buffer[LIGHTGUN_CROSSHAIR_SIZE * LIGHTGUN_CROSSHAIR_SIZE * 4];
		u8 *dst = temp_tex_buffer;
		u8* colour = &lightgun_palette[lightgun_params[i].colour * 3];
		u8 *src = lightgun_img_crosshair;

		for (int y = 0; y < LIGHTGUN_CROSSHAIR_SIZE; y++)
		{
			for (int x = 0; x < LIGHTGUN_CROSSHAIR_SIZE; x++)
			{
				if (*src++)
				{
					*dst++ = colour[0];
					*dst++ = colour[1];
					*dst++ = colour[2];
					*dst++ = 0xFF;
				}
				else
				{
					*dst++ = 0;
					*dst++ = 0;
					*dst++ = 0;
					*dst++ = 0;
				}
			}
		}
		texture->tex_type = TextureType::_8888;
		texture->SetDevice(context->GetDevice());
		texture->SetPhysicalDevice(context->GetPhysicalDevice());
		commandBuffers[context->GetCurrentImageIndex()].push_back(commandPool->Allocate());
		texture->SetCommandBuffer(commandBuffers[context->GetCurrentImageIndex()].back());
		texture->UploadToGPU(LIGHTGUN_CROSSHAIR_SIZE, LIGHTGUN_CROSSHAIR_SIZE, temp_tex_buffer, false);
		texture->SetCommandBuffer(nullptr);
		lightgun_params[i].dirty = false;
	}
	return &commandBuffers[context->GetCurrentImageIndex()];
}

void VulkanOSD::DrawOSD(vk::CommandBuffer commandBuffer, vk::Extent2D viewport)
{
	pipeline->BindPipeline(commandBuffer, false);
	QuadVertex vtx[] = {
		{ { -1.f, -1.f, 0.f }, { 0.f, 1.f } },
		{ {  1.f, -1.f, 0.f }, { 1.f, 1.f } },
		{ { -1.f,  1.f, 0.f }, { 0.f, 0.f } },
		{ {  1.f,  1.f, 0.f }, { 1.f, 0.f } },
	};

	for (int i = 0; i < VMU_COUNT; i++)
	{
		if (!vmuTextures[i])
			continue;

		const auto& vmu_screen_param = vmu_screen_params[i];
		float x = 0;
		float y = 0;
		float w = VMU_SCREEN_WIDTH * vmu_screen_param.vmu_screen_size_mult;
		float h = VMU_SCREEN_HEIGHT * vmu_screen_param.vmu_screen_size_mult;

		switch (vmu_screen_param.vmu_screen_position)
		{
			case UPPER_LEFT:
				x = 0;
				y = 0;
				break;

			case UPPER_RIGHT:
				x = viewport.width - w;
				y = 0;
				break;

			case LOWER_LEFT:
				x = 0;
				y = viewport.height - h;
				break;

			case LOWER_RIGHT:
				x = viewport.width - w;
				y = viewport.height - h;
				break;
		}

		vk::Viewport viewport(x, y, w, h);
		commandBuffer.setViewport(0, 1, &viewport);
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(w, h)));

		float alpha = vmu_screen_param.vmu_screen_opacity / 255.f;
		float blendConstants[4] = { alpha, alpha, alpha, alpha };
		commandBuffer.setBlendConstants(blendConstants);

		drawers[i]->Draw(commandBuffer, vmuTextures[i]->GetImageView(), vtx, true);
	}

	pipeline->BindPipeline(commandBuffer, true);
	for (int i = 0; i < XHAIR_COUNT; i++)
	{
		if (!xhairTextures[i])
			continue;

		float w = LIGHTGUN_CROSSHAIR_SIZE;
		float h = LIGHTGUN_CROSSHAIR_SIZE;
		float x = lightgun_params[i].x * viewport.width / 640 - w / 2;
		float y = lightgun_params[i].y * viewport.height / 480 - h / 2;

		vk::Viewport viewport(x, y, w, h);
		commandBuffer.setViewport(0, 1, &viewport);
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(w, h)));

		float blendConstants[4] = { 1.f, 1.f, 1.f, 1.f };
		commandBuffer.setBlendConstants(blendConstants);

		xhairDrawers[i]->Draw(commandBuffer, xhairTextures[i]->GetImageView(), vtx, true);
	}
}
