#include "ViewportManager.hpp"
#include "DeletionQueue.hpp"

#include "DynamicDispatch.hpp"
#include "GlobUtils.hpp"
#include "GuiResourceManager.hpp"

#include <DEngine/Gfx/detail/Assert.hpp>
#include <DEngine/Std/Utility.hpp>

namespace DEngine::Gfx::Vk
{
	[[nodiscard]] static GfxRenderTarget InitializeGfxViewportRenderTarget(
		GlobUtils const& globUtils,
		ViewportID viewportID,
		vk::Extent2D viewportSize);

	static void TransitionGfxImage(
		DeviceDispatch const& device,
		DeletionQueue const& deletionQueue,
		QueueData const& queues,
		vk::Image img,
		bool useEditorPipeline);

	// Assumes the viewportManager.viewportDatas is already locked.
	static void HandleViewportRenderTargetInitialization(
		GfxRenderTarget& renderTarget,
		GlobUtils const& globUtils,
		ViewportUpdate updateData)
	{
		vk::Result vkResult{};

		// We need to create this virtual viewport
		renderTarget = InitializeGfxViewportRenderTarget(
			globUtils,
			updateData.id,
			{ updateData.width, updateData.height });
	}

	// Assumes the viewportManager.viewportDatas is already locked.
	static void HandleViewportResize(
		GfxRenderTarget& renderTarget,
		GlobUtils const& globUtils,
		ViewportUpdate updateData)
	{
		// This image needs to be resized. We just make a new one and push a job to delete the old one.
		// VMA should handle memory allocations to be effective and reused.
		globUtils.deletionQueue.Destroy(renderTarget.framebuffer);
		globUtils.deletionQueue.Destroy(renderTarget.imgView);
		globUtils.deletionQueue.Destroy(renderTarget.vmaAllocation, renderTarget.img);

		renderTarget = InitializeGfxViewportRenderTarget(
			globUtils,
			updateData.id,
			{ updateData.width, updateData.height });
	}

	static ViewportData InitializeViewport(
		GlobUtils const& globUtils,
		ViewportManager::CreateJob createJob,
		uSize camElementSize,
		vk::DescriptorSetLayout cameraDescrLayout)
	{
		vk::Result vkResult{};

		ViewportData viewport{};

		// Make the command buffer stuff
		vk::CommandPoolCreateInfo cmdPoolInfo{};
		cmdPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		cmdPoolInfo.queueFamilyIndex = globUtils.queues.graphics.FamilyIndex();
		viewport.cmdPool = globUtils.device.createCommandPool(cmdPoolInfo);
		if (globUtils.UsingDebugUtils())
		{
			std::string name = std::string("Graphics viewport #") + std::to_string((u64)createJob.id) + " - Cmd Pool";
			globUtils.debugUtils.Helper_SetObjectName(
				globUtils.device.handle,
				viewport.cmdPool,
				name.c_str());
		}

		vk::CommandBufferAllocateInfo cmdBufferAllocInfo{};
		cmdBufferAllocInfo.commandBufferCount = globUtils.inFlightCount;
		cmdBufferAllocInfo.commandPool = viewport.cmdPool;
		cmdBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
		viewport.cmdBuffers.Resize(cmdBufferAllocInfo.commandBufferCount);
		vkResult = globUtils.device.allocateCommandBuffers(cmdBufferAllocInfo, viewport.cmdBuffers.Data());
		if (vkResult != vk::Result::eSuccess)
			throw std::runtime_error("DEngine, Vulkan: Unable to allocate command buffers for viewport.");
		if (globUtils.UsingDebugUtils())
		{
			for (uSize i = 0; i < viewport.cmdBuffers.Size(); i += 1)
			{
				std::string name = std::string("Graphics viewport #") + std::to_string((u64)createJob.id) + " - CmdBuffer #" + std::to_string(i);
				globUtils.debugUtils.Helper_SetObjectName(
					globUtils.device.handle,
					viewport.cmdBuffers[i],
					name.c_str());
			}
		}

		// Make the descriptorpool and sets
		vk::DescriptorPoolSize descrPoolSize{};
		descrPoolSize.type = vk::DescriptorType::eUniformBuffer;
		descrPoolSize.descriptorCount = globUtils.inFlightCount;

		vk::DescriptorPoolCreateInfo descrPoolInfo{};
		descrPoolInfo.maxSets = globUtils.inFlightCount;
		descrPoolInfo.poolSizeCount = 1;
		descrPoolInfo.pPoolSizes = &descrPoolSize;
		viewport.cameraDescrPool = globUtils.device.createDescriptorPool(descrPoolInfo);
		if (globUtils.UsingDebugUtils())
		{
			std::string name = std::string("Graphics viewport #") + std::to_string((u64)createJob.id) +
				" - Camera-data DescrPool";
			globUtils.debugUtils.Helper_SetObjectName(
				globUtils.device.handle,
				viewport.cameraDescrPool,
				name.c_str());
		}

		
		vk::DescriptorSetAllocateInfo descrSetAllocInfo{};
		descrSetAllocInfo.descriptorPool = viewport.cameraDescrPool;
		descrSetAllocInfo.descriptorSetCount = globUtils.inFlightCount;
		Std::StackVec<vk::DescriptorSetLayout, Constants::maxInFlightCount> descrLayouts;
		descrLayouts.Resize(globUtils.inFlightCount);
		for (auto& item : descrLayouts)
			item = cameraDescrLayout;
		descrSetAllocInfo.pSetLayouts = descrLayouts.Data();
		viewport.camDataDescrSets.Resize(globUtils.inFlightCount);
		vkResult = globUtils.device.allocateDescriptorSets(descrSetAllocInfo, viewport.camDataDescrSets.Data());
		if (vkResult != vk::Result::eSuccess)
			throw std::runtime_error("DEngine - Vulkan: Unable to allocate descriptor sets for viewport camera data.");
		if (globUtils.UsingDebugUtils())
		{
			for (uSize i = 0; i < viewport.camDataDescrSets.Size(); i += 1)
			{
				std::string name = std::string("Graphics viewport #") + std::to_string((u64)createJob.id) +
					" - Camera-data DescrSet #" + std::to_string(i);
				globUtils.debugUtils.Helper_SetObjectName(
					globUtils.device.handle,
					viewport.camDataDescrSets[i],
					name.c_str());
			}
		}


		// Allocate the data
		vk::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sharingMode = vk::SharingMode::eExclusive;
		bufferCreateInfo.size = globUtils.inFlightCount * camElementSize;
		bufferCreateInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
		VmaAllocationCreateInfo memAllocInfo{};
		memAllocInfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
		memAllocInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;
		VmaAllocationInfo resultMemInfo{};
		vkResult = (vk::Result)vmaCreateBuffer(
			globUtils.vma,
			(VkBufferCreateInfo*)&bufferCreateInfo,
			&memAllocInfo,
			(VkBuffer*)&viewport.camDataBuffer,
			&viewport.camVmaAllocation,
			&resultMemInfo);
		if (vkResult != vk::Result::eSuccess)
			throw std::runtime_error("DEngine - Vulkan: VMA unable to allocate cam-data memory for viewport.");
		viewport.camDataMappedMem = { (u8*)resultMemInfo.pMappedData, (uSize)resultMemInfo.size };
		if (globUtils.UsingDebugUtils())
		{
			std::string name = std::string("Graphics viewport #") + std::to_string((u64)createJob.id) + " - CamData Buffer";
			globUtils.debugUtils.Helper_SetObjectName(
				globUtils.device.handle,
				viewport.camDataBuffer,
				name.c_str());
		}
		

		// Update descriptor sets
		Std::StackVec<vk::WriteDescriptorSet, Constants::maxInFlightCount> writes{};
		writes.Resize(globUtils.inFlightCount);
		Std::StackVec<vk::DescriptorBufferInfo, Constants::maxInFlightCount> bufferInfos{};
		bufferInfos.Resize(globUtils.inFlightCount);
		for (uSize i = 0; i < globUtils.inFlightCount; i += 1)
		{
			vk::WriteDescriptorSet& writeData = writes[i];
			vk::DescriptorBufferInfo& bufferInfo = bufferInfos[i];

			writeData.descriptorCount = 1;
			writeData.descriptorType = vk::DescriptorType::eUniformBuffer;
			writeData.dstBinding = 0;
			writeData.dstSet = viewport.camDataDescrSets[i];
			writeData.pBufferInfo = &bufferInfo;

			bufferInfo.buffer = viewport.camDataBuffer;
			bufferInfo.offset = camElementSize * i;
			bufferInfo.range = camElementSize;
		}
		globUtils.device.updateDescriptorSets({ (u32)writes.Size(), writes.Data() }, nullptr);

		return viewport;
	}

	void HandleViewportDeleteJobs(
		ViewportManager& viewportManager,
		GlobUtils const& globUtils)
	{
		std::lock_guard lock{ viewportManager.deleteQueue_Lock };

		// Execute pending deletions
		for (ViewportID id : viewportManager.deleteQueue)
		{
			auto const viewportDataNodeIt = std::find_if(
				viewportManager.viewportNodes.begin(),
				viewportManager.viewportNodes.end(),
				[id](Std::Trait::RemoveCVRef<decltype(viewportManager.viewportNodes[0])> const& val) -> bool { return id == val.id; });
			DENGINE_DETAIL_GFX_ASSERT(viewportDataNodeIt != viewportManager.viewportNodes.end());
			auto& viewportNode = *viewportDataNodeIt;
			ViewportData& viewportData = viewportDataNodeIt->viewport;

			// Delete all the resources
			globUtils.deletionQueue.Destroy(viewportData.cameraDescrPool);
			globUtils.deletionQueue.Destroy(viewportData.camVmaAllocation, viewportData.camDataBuffer);
			globUtils.deletionQueue.Destroy(viewportData.cmdPool);
			globUtils.deletionQueue.Destroy(viewportData.renderTarget.framebuffer);
			globUtils.deletionQueue.Destroy(viewportData.renderTarget.imgView);
			globUtils.deletionQueue.Destroy(viewportData.renderTarget.vmaAllocation, viewportData.renderTarget.img);

			// Swap with back, remove the element.
			std::swap(viewportNode, viewportManager.viewportNodes.back());
			viewportManager.viewportNodes.pop_back();
		}
		viewportManager.deleteQueue.clear();
	}

	void HandleViewportCreationJobs(
		ViewportManager& viewportManager,
		GlobUtils const& globUtils)
	{
		vk::Result vkResult{};

		std::lock_guard lock{ viewportManager.createQueue_Lock };

		for (ViewportManager::CreateJob const& createJob : viewportManager.createQueue)
		{
			ViewportData newViewport = InitializeViewport(
				globUtils,
				createJob,
				viewportManager.camElementSize,
				viewportManager.cameraDescrLayout);

			viewportManager.viewportNodes.push_back({ createJob.id, newViewport });
		}
		viewportManager.createQueue.clear();
	}
}

using namespace DEngine;
using namespace DEngine::Gfx;

bool Vk::ViewportManager::Init(
	ViewportManager& manager,
	DeviceDispatch const& device,
	uSize minUniformBufferOffsetAlignment,
	DebugUtilsDispatch const* debugUtils)
{
	vk::DeviceSize elementSize = 64;
	if (minUniformBufferOffsetAlignment > elementSize)
		elementSize = minUniformBufferOffsetAlignment;
	manager.camElementSize = (uSize)elementSize;

	vk::DescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorCount = 1;
	binding.descriptorType = vk::DescriptorType::eUniformBuffer;
	binding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	vk::DescriptorSetLayoutCreateInfo descrLayoutInfo{};
	descrLayoutInfo.bindingCount = 1;
	descrLayoutInfo.pBindings = &binding;
	manager.cameraDescrLayout = device.createDescriptorSetLayout(descrLayoutInfo);
	if (debugUtils)
	{
		debugUtils->Helper_SetObjectName(
			device.handle,
			manager.cameraDescrLayout,
			"ViewportManager - Cam DescrLayout");
	}

	return true;
}

void Vk::ViewportManager::NewViewport(
	ViewportManager& viewportManager,
	ViewportID& viewportID)
{
	ViewportManager::CreateJob createJob{};

	std::lock_guard lock{ viewportManager.createQueue_Lock };
	createJob.id = (ViewportID)viewportManager.viewportIDTracker;
	viewportID = createJob.id;

	viewportManager.createQueue.push_back(createJob);
	viewportManager.viewportIDTracker += 1;
}

void Vk::ViewportManager::DeleteViewport(
	ViewportManager& viewportManager,
	ViewportID viewportId)
{
	std::lock_guard lock{ viewportManager.deleteQueue_Lock };
	viewportManager.deleteQueue.push_back(viewportId);
}

void Vk::ViewportManager::HandleEvents(
	ViewportManager& viewportManager,
	GlobUtils const& globUtils,
	Std::Span<ViewportUpdate const> viewportUpdates,
	GuiResourceManager const& guiResourceManager)
{
	vk::Result vkResult{};

	HandleViewportDeleteJobs(viewportManager, globUtils);
	HandleViewportCreationJobs(viewportManager, globUtils);

	// First we handle any viewportManager that need to be initialized, not resized.
	for (Gfx::ViewportUpdate const& updateData : viewportUpdates)
	{
		auto const viewportDataNodeIt = Std::FindIf(
			viewportManager.viewportNodes.begin(),
			viewportManager.viewportNodes.end(),
			[&updateData](Std::Trait::RemoveCVRef<decltype(viewportManager.viewportNodes[0])> const& val) -> bool {return updateData.id == val.id; });
		DENGINE_DETAIL_GFX_ASSERT(viewportDataNodeIt != viewportManager.viewportNodes.end());
		ViewportData& viewportData = viewportDataNodeIt->viewport;
		if (viewportData.renderTarget.img == vk::Image())
		{
			// This image is not initalized.
			HandleViewportRenderTargetInitialization(
				viewportData.renderTarget,
				globUtils, 
				updateData);

			// WARNING! BAD CODE HERE I THINK
			vk::DescriptorSetAllocateInfo descrSetAllocInfo{};
			descrSetAllocInfo.descriptorPool = guiResourceManager.viewportDescrPool;
			descrSetAllocInfo.descriptorSetCount = 1;
			descrSetAllocInfo.pSetLayouts = &guiResourceManager.viewportDescrSetLayout;
			vkResult = globUtils.device.allocateDescriptorSets(descrSetAllocInfo, &viewportData.descrSet);
			if (vkResult != vk::Result::eSuccess)
				throw std::runtime_error("DEngine - Vulkan: Unable to allocate descr set for viewport image.");

			vk::DescriptorImageInfo descrImgInfo{};
			descrImgInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			descrImgInfo.imageView = viewportData.renderTarget.imgView;
			descrImgInfo.sampler = guiResourceManager.font_sampler;
			vk::WriteDescriptorSet descrWrite{};
			descrWrite.descriptorCount = 1;
			descrWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			descrWrite.dstSet = viewportData.descrSet;
			descrWrite.pImageInfo = &descrImgInfo;
			globUtils.device.updateDescriptorSets(descrWrite, nullptr);
		}
		else if (updateData.width != viewportData.renderTarget.extent.width ||
			updateData.height != viewportData.renderTarget.extent.height)
		{
			// This image needs to be resized.
			HandleViewportResize(
				viewportData.renderTarget,
				globUtils, 
				updateData);

			globUtils.device.waitIdle();
			vk::DescriptorImageInfo descrImgInfo{};
			descrImgInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			descrImgInfo.imageView = viewportData.renderTarget.imgView;
			descrImgInfo.sampler = guiResourceManager.font_sampler;
			vk::WriteDescriptorSet descrWrite{};
			descrWrite.descriptorCount = 1;
			descrWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			descrWrite.dstSet = viewportData.descrSet;
			descrWrite.pImageInfo = &descrImgInfo;
			globUtils.device.updateDescriptorSets(descrWrite, nullptr);
		}
	}
}

void Vk::ViewportManager::UpdateCameras(
	ViewportManager& viewportManager,
	GlobUtils const& globUtils,
	Std::Span<ViewportUpdate const> viewportUpdates,
	u8 inFlightIndex)
{
	for (auto const& viewportUpdate : viewportUpdates)
	{
		auto nodeIt = Std::FindIf(
			viewportManager.viewportNodes.begin(),
			viewportManager.viewportNodes.end(),
			[&viewportUpdate](Std::Trait::RemoveCVRef<decltype(viewportManager.viewportNodes[0])> const& val) -> bool {
				return viewportUpdate.id == val.id; });
		DENGINE_DETAIL_GFX_ASSERT(nodeIt != viewportManager.viewportNodes.end());
		auto& node = *nodeIt;
		DENGINE_DETAIL_GFX_ASSERT(viewportManager.camElementSize * inFlightIndex < node.viewport.camDataMappedMem.Size());
		std::memcpy(
			node.viewport.camDataMappedMem.Data() + viewportManager.camElementSize * inFlightIndex,
			&viewportUpdate.transform,
			viewportManager.camElementSize);
	}
}

Vk::GfxRenderTarget Vk::InitializeGfxViewportRenderTarget(
	GlobUtils const& globUtils,
	ViewportID viewportID,
	vk::Extent2D viewportSize)
{
	vk::Result vkResult{};

	GfxRenderTarget returnVal{};
	returnVal.extent = viewportSize;

	vk::ImageCreateInfo imageInfo{};
	imageInfo.arrayLayers = 1;
	imageInfo.extent = vk::Extent3D{ viewportSize.width, viewportSize.height, 1 };
	imageInfo.format = vk::Format::eR8G8B8A8Srgb;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.mipLevels = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
	if (globUtils.editorMode)
	{
		// We want to sample from the image to show it in the editor.
		imageInfo.usage |= vk::ImageUsageFlagBits::eSampled;
	}

	VmaAllocationCreateInfo vmaAllocInfo{};
	//vmaAllocInfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	vmaAllocInfo.memoryTypeBits = 0;
	vmaAllocInfo.pool = 0;
	vmaAllocInfo.preferredFlags = 0;
	vmaAllocInfo.pUserData = nullptr;
	vmaAllocInfo.requiredFlags = 0;
	vmaAllocInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

	vkResult = (vk::Result)vmaCreateImage(
		globUtils.vma,
		(VkImageCreateInfo*)&imageInfo,
		&vmaAllocInfo,
		(VkImage*)&returnVal.img,
		&returnVal.vmaAllocation,
		nullptr);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("DEngine - Vulkan: Could not make VkImage through VMA when initializing virtual viewport.");

	if (globUtils.UsingDebugUtils())
	{
		std::string name = std::string("Graphics viewport #") + std::to_string((u64)viewportID) + " - Image";
		globUtils.debugUtils.Helper_SetObjectName(
			globUtils.device.handle,
			returnVal.img,
			name.c_str());
	}

	// We have to transition this image
	TransitionGfxImage(
		globUtils.device,
		globUtils.deletionQueue,
		globUtils.queues,
		returnVal.img,
		globUtils.editorMode);

	// Make the image view
	vk::ImageViewCreateInfo imgViewInfo{};
	imgViewInfo.components.r = vk::ComponentSwizzle::eIdentity;
	imgViewInfo.components.g = vk::ComponentSwizzle::eIdentity;
	imgViewInfo.components.b = vk::ComponentSwizzle::eIdentity;
	imgViewInfo.components.a = vk::ComponentSwizzle::eIdentity;
	imgViewInfo.format = vk::Format::eR8G8B8A8Srgb;
	imgViewInfo.image = returnVal.img;
	imgViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	imgViewInfo.subresourceRange.baseArrayLayer = 0;
	imgViewInfo.subresourceRange.baseMipLevel = 0;
	imgViewInfo.subresourceRange.layerCount = 1;
	imgViewInfo.subresourceRange.levelCount = 1;
	imgViewInfo.viewType = vk::ImageViewType::e2D;

	returnVal.imgView = globUtils.device.createImageView(imgViewInfo);
	if (globUtils.UsingDebugUtils())
	{
		std::string name = std::string("Graphics viewport #") + std::to_string((u64)viewportID) + " - Image View";
		globUtils.debugUtils.Helper_SetObjectName(
			globUtils.device.handle,
			returnVal.imgView,
			name.c_str());
	}

	vk::FramebufferCreateInfo fbInfo{};
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &returnVal.imgView;
	fbInfo.height = viewportSize.height;
	fbInfo.layers = 1;
	fbInfo.renderPass = globUtils.gfxRenderPass;
	fbInfo.width = viewportSize.width;
	returnVal.framebuffer = globUtils.device.createFramebuffer(fbInfo);
	if (globUtils.UsingDebugUtils())
	{
		std::string name = std::string("Graphics viewport #") + std::to_string((u64)viewportID) + " - Framebuffer";
		globUtils.debugUtils.Helper_SetObjectName(
			globUtils.device.handle,
			returnVal.imgView,
			name.c_str());
	}

	return returnVal;
}


void Vk::TransitionGfxImage(
	DeviceDispatch const& device,
	DeletionQueue const& deletionQueue,
	QueueData const& queues,
	vk::Image img,
	bool useEditorPipeline)
{
	vk::Result vkResult{};

	vk::CommandPoolCreateInfo cmdPoolInfo{};
	cmdPoolInfo.queueFamilyIndex = queues.graphics.FamilyIndex();
	vk::CommandPool cmdPool = device.createCommandPool(cmdPoolInfo);

	vk::CommandBufferAllocateInfo cmdBufferAllocInfo{};
	cmdBufferAllocInfo.commandBufferCount = 1;
	cmdBufferAllocInfo.commandPool = cmdPool;
	cmdBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	vk::CommandBuffer cmdBuffer{};
	vkResult = device.allocateCommandBuffers(cmdBufferAllocInfo, &cmdBuffer);
	if (vkResult != vk::Result::eSuccess)
		throw std::runtime_error("Unable to allocate command buffer when transitioning swapchainData images.");

	// Record commandbuffer
	{
		vk::CommandBufferBeginInfo cmdBeginInfo{};
		cmdBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
		device.beginCommandBuffer(cmdBuffer, cmdBeginInfo);
		vk::ImageMemoryBarrier imgBarrier{};
		imgBarrier.image = img;
		imgBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imgBarrier.subresourceRange.layerCount = 1;
		imgBarrier.subresourceRange.levelCount = 1;
		imgBarrier.oldLayout = vk::ImageLayout::eUndefined;
		// If we're in editor mode, we want to sample from the graphics viewport
		// into the editor's GUI pass.
		// If we're not in editor mode, we use a render-pass where this
		// is the image that gets copied onto the swapchain. That render-pass
		// requires the image to be in transferSrcOptimal layout.
		if (useEditorPipeline)
			imgBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		else
			imgBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		imgBarrier.srcAccessMask = {};
		// We want to write to the image as a render-target
		imgBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

		vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

		device.cmdPipelineBarrier(
			cmdBuffer,
			srcStage,
			dstStage,
			vk::DependencyFlags{},
			{},
			{},
			{ 1, &imgBarrier });

		device.endCommandBuffer(cmdBuffer);
	}

	vk::FenceCreateInfo fenceInfo{};
	vk::Fence fence = device.createFence(fenceInfo);

	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	queues.graphics.submit(submitInfo, fence);

	deletionQueue.Destroy(fence, cmdPool);
}