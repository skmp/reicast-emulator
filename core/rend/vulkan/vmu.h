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
#pragma once
#include <array>
#include <memory>
#include "quad.h"
#include "commandpool.h"

class Texture;
#define VMU_COUNT 4
#define XHAIR_COUNT (ARRAY_SIZE(lightgun_params))

class VulkanOSD
{
public:
	~VulkanOSD();
	void Init(QuadPipeline *pipeline) {
		this->pipeline = pipeline;
		for (auto& drawer : drawers)
		{
			drawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());
			drawer->Init(pipeline);
		}
		for (auto& drawer : xhairDrawers)
		{
			drawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());
			drawer->Init(pipeline);
		}
	}
	const std::vector<vk::CommandBuffer>* PrepareOSD(CommandPool *commandPool);
	void DrawOSD(vk::CommandBuffer commandBuffer, vk::Extent2D viewport);

private:
	std::array<std::unique_ptr<Texture>, VMU_COUNT> vmuTextures;
	std::array<std::unique_ptr<Texture>, XHAIR_COUNT> xhairTextures;
	std::vector<std::vector<vk::CommandBuffer>> commandBuffers;
	std::array<std::unique_ptr<QuadDrawer>, VMU_COUNT> drawers;
	std::array<std::unique_ptr<QuadDrawer>, XHAIR_COUNT> xhairDrawers;
	QuadPipeline *pipeline = nullptr;
};
