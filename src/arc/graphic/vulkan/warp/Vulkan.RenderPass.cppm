module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.RenderPass;

import Core.Vulkan.Dependency;
import Core.Vulkan.Comp;
import std;

export namespace Core::Vulkan{
	namespace
	class RenderPass : public Wrapper<VkRenderPass>{
	public:
		Dependency<VkDevice> device{};

		std::vector<VkAttachmentDescription> attachmentSockets{};

		struct AttachmentReference{
			std::vector<VkAttachmentReference> color{};

			std::optional<std::vector<VkAttachmentReference>> input{};
			std::optional<VkAttachmentReference> resolve{};
			std::optional<VkAttachmentReference> depthStencil{};

			std::optional<std::vector<std::uint32_t>> reserved{};
		};

		struct SubpassData{
			VkSubpassDescription description{};
			std::vector<VkSubpassDependency> dependencies{};

			AttachmentReference attachment{};

			void setProperties(VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			                     VkSubpassDescriptionFlags flags = 0){
				description.flags = flags;
				description.pipelineBindPoint = bindPoint;
			}

			void bindAttachments(){
				description.colorAttachmentCount = attachment.color.size();
				description.pColorAttachments = attachment.color.data();

				if(attachment.input){
					description.inputAttachmentCount = attachment.input->size();
					description.pInputAttachments = attachment.input->data();
				}else{
					description.inputAttachmentCount = 0;
					description.pInputAttachments = nullptr;
				}

				if(attachment.reserved){
					description.preserveAttachmentCount = attachment.reserved->size();
					description.pPreserveAttachments = attachment.reserved->data();
				}else{
					description.preserveAttachmentCount = 0;
					description.pPreserveAttachments = nullptr;
				}

				if(attachment.depthStencil){
					description.pDepthStencilAttachment = &attachment.depthStencil.value();
				}else{
					description.pDepthStencilAttachment = nullptr;
				}

				if(attachment.resolve){
					description.pResolveAttachments = &attachment.resolve.value();
				}else{
					description.pResolveAttachments = nullptr;
				}
			}
		};

		std::vector<SubpassData> subpasses{};


		[[nodiscard]] RenderPass() = default;

		~RenderPass(){
			if(device)vkDestroyRenderPass(device, handle, nullptr);
		}

		RenderPass(const RenderPass& other) = delete;

		RenderPass(RenderPass&& other) noexcept = default;

		RenderPass& operator=(const RenderPass& other) = delete;

		RenderPass& operator=(RenderPass&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyRenderPass(device, handle, nullptr);
			Wrapper<VkRenderPass>::operator =(std::move(other));
			device = std::move(other.device);
			return *this;
		}

		[[nodiscard]] auto subpassSize() const noexcept{
			return subpasses.size();
		}

		void pushAttachment(const VkAttachmentDescription& description){
			attachmentSockets.push_back(description);
		}

		void createRenderPass(){
			for (auto& subpassData : subpasses){
				subpassData.bindAttachments();
			}

			const auto subpassDesc = subpasses
				| std::views::transform(&SubpassData::description)
				| std::ranges::to<std::vector>();

			const auto subpassDependency = subpasses
				| std::views::transform(&SubpassData::dependencies)
				| std::views::join
				| std::ranges::to<std::vector>();

			// VkAttachmentDescription colorAttachment{};
			// colorAttachment.format = swapChain.getFormat();
			// colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			// colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //LOAD;
			// colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			// colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			// colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			// colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //TODO
			// colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			const VkRenderPassCreateInfo renderPassInfo{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
					.pNext = nullptr,
					.flags = 0,
					.attachmentCount = static_cast<std::uint32_t>(attachmentSockets.size()),
					.pAttachments = attachmentSockets.data(),
					.subpassCount = static_cast<std::uint32_t>(subpassDesc.size()),
					.pSubpasses = subpassDesc.data(),
					.dependencyCount = static_cast<std::uint32_t>(subpassDependency.size()),
					.pDependencies = subpassDependency.data()
				};

			if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create render pass!");
			}
		}
	};
}
