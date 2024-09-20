module;

#include <vulkan/vulkan.h>

export module Graphic.PostProcessor;

export import Core.Vulkan.Context;
export import Core.Vulkan.Buffer.FrameBuffer;
export import Core.Vulkan.RenderProcedure;
export import Core.Vulkan.Buffer.CommandBuffer;
export import Core.Vulkan.Semaphore;
export import Core.Vulkan.Preinstall;
export import Core.Vulkan.Util;

import Geom.Vector2D;
import std;

export namespace Graphic{

	//OPTM considering the small use of these fields, maybe vector is better?
	struct AttachmentPort{
		std::unordered_map<std::uint32_t, VkImageView> views{};
		std::unordered_map<std::uint32_t, VkImage> images{};

		void addAttachment(const std::uint32_t index, const Core::Vulkan::Attachment& attachment){
			views.insert_or_assign(index, attachment.getView());
			images.insert_or_assign(index, attachment.getImage());
		}

		template <std::ranges::range Rng>
			requires std::convertible_to<std::ranges::range_reference_t<Rng>, const Core::Vulkan::Attachment&>
		void addAttachment(Rng&& attachments){
			for(auto [index, attachment] : std::as_const(attachments) | std::views::enumerate){
				views.insert_or_assign(index, attachment.getView());
				images.insert_or_assign(index, attachment.getImage());
			}
		}
	};

	using PortProv = std::move_only_function<AttachmentPort()>;
	using DescriptorLayoutProv = std::move_only_function<std::vector<VkDescriptorSetLayout>()>;

	struct PostProcessor{
		const Core::Vulkan::Context* context{};

		AttachmentPort port{};
		PortProv portProv{};

		Core::Vulkan::CommandBuffer mainCommandBuffer{};

		[[nodiscard]] PostProcessor() = default;

		[[nodiscard]] explicit PostProcessor(
			const Core::Vulkan::Context& context,
			PortProv&& portProv = {})
			: context{&context},
			  portProv{std::move(portProv)}{
			if(this->portProv){
				port = this->portProv();
			}
		}

		void resize(Geom::USize2 size) = delete;

		[[nodiscard]] Geom::USize2 size() const = delete;

		void submitCommand() const{
			Core::Vulkan::Util::submitCommand(
				context->device.getPrimaryComputeQueue(),
				mainCommandBuffer
				);
		}
	};

	struct ComputePostProcessor : PostProcessor{
		Core::Vulkan::SinglePipelineData pipelineData{};
		std::vector<Core::Vulkan::Attachment> images{};

		std::vector<Core::Vulkan::UniformBuffer> uniformBuffers{};
		std::vector<Core::Vulkan::DescriptorBuffer> descriptorBuffers{};

		DescriptorLayoutProv layoutProv{};

		std::move_only_function<void(VkCommandBuffer)> commandRecorderAdditional{};

		std::move_only_function<void(ComputePostProcessor&)> commandRecorder{};
		std::move_only_function<void(ComputePostProcessor&)> descriptorSetUpdator{};
		std::move_only_function<void(ComputePostProcessor&)> resizeCallback{};

		[[nodiscard]] ComputePostProcessor() = default;

		[[nodiscard]] explicit ComputePostProcessor(const Core::Vulkan::Context& context, PortProv&& portProv, DescriptorLayoutProv&& layoutProv = {})
		: PostProcessor{context, std::move(portProv)}, pipelineData{&context}, layoutProv{std::move(layoutProv)}{

		}

		template <std::derived_from<Core::Vulkan::Attachment> T = Core::Vulkan::Attachment, std::invocable<ComputePostProcessor&, T&> Func>
		void addImage(Func&& creator){
			T image{context->physicalDevice, context->device};
			creator(*this, image);

			images.push_back(std::move(static_cast<Core::Vulkan::Attachment&&>(image)));
		}

		void bindAppendedDescriptors(VkCommandBuffer commandBuffer){
			commandRecorderAdditional(commandBuffer);
		}

		void createDescriptorBuffers(const std::uint32_t size){
			descriptorBuffers.reserve(size);
			for(std::size_t i = 0; i < size; ++i){
				descriptorBuffers.push_back(Core::Vulkan::DescriptorBuffer{
					context->physicalDevice, context->device, pipelineData.descriptorSetLayout, pipelineData.descriptorSetLayout.size()
				});
			}
		}

		void addUniformBuffer(const std::size_t size, const VkBufferUsageFlags otherUsages = 0){
			uniformBuffers.push_back(Core::Vulkan::UniformBuffer{context->physicalDevice, context->device, size, otherUsages});
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
		void addUniformBuffer(const T& data, const VkBufferUsageFlags otherUsages = 0){
			uniformBuffers.push_back(Core::Vulkan::UniformBuffer{context->physicalDevice, context->device, sizeof(T), otherUsages});
			uniformBuffers.back().memory.loadData<T>(data, 0);
		}

		std::vector<VkDescriptorSetLayout> getAppendedLayout(){
			if(layoutProv)return layoutProv();
			return {};
		}

		[[nodiscard]] Geom::USize2 size() const{
			return pipelineData.size;
		}

		void updateDescriptors(){
			if(descriptorSetUpdator)descriptorSetUpdator(*this);
		}

		void recordCommand(Core::Vulkan::CommandBuffer&& mainCommandBuffer){
			this->mainCommandBuffer = std::move(mainCommandBuffer);
			if(commandRecorder)commandRecorder(*this);
		}

		void resize(Geom::USize2 size, VkCommandBuffer commandBuffer, Core::Vulkan::CommandBuffer&& mainCommandBuffer){
			if(size == this->size())return;
			pipelineData.size = size;
			if(resizeCallback)resizeCallback(*this);

			if(portProv){
				port = portProv();
			}

			if(!commandBuffer)return;

			for (auto && localAttachment : images){
				localAttachment.resize(size, commandBuffer);
			}

			updateDescriptors();
			recordCommand(std::move(mainCommandBuffer));
		}

	};

	struct PostProcessorCreateProperty{
		Geom::USize2 size{};
		PortProv portProv{};
		DescriptorLayoutProv layoutProv{};
		Core::Vulkan::TransientCommand createInitCommandBuffer{};
		Core::Vulkan::CommandBuffer mainCommandBuffer{};
	};

	struct ComputePostProcessorFactory{
		const Core::Vulkan::Context* context{};
		std::function<ComputePostProcessor(const ComputePostProcessorFactory&, PostProcessorCreateProperty&&)> creator{};

		ComputePostProcessor generate(PostProcessorCreateProperty&& property = {}) const{
			return creator(*this, std::move(property));
		}
	};
}
