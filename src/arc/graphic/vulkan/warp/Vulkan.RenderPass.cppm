module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.RenderPass;

import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{

	class RenderPass : public Wrapper<VkRenderPass>{
	public:
		Dependency<VkDevice> device{};

		std::vector<VkAttachmentDescription> attachmentSockets{};

		struct AttachmentReference{
			enum struct Category : std::uint32_t{
				input, color, depthStencil, reserved, resolve
			};

			std::vector<VkAttachmentReference> color{};

			std::optional<std::vector<VkAttachmentReference>> input{};
			std::optional<VkAttachmentReference> resolve{};
			std::optional<VkAttachmentReference> depthStencil{};

			std::optional<std::vector<std::uint32_t>> reserved{};

			std::uint32_t getMaxIndex(){
				const std::uint32_t max1 = input.has_value() ? std::ranges::max(input.value(), std::less<std::uint32_t>{}, &VkAttachmentReference::attachment).attachment : 0;
				const std::uint32_t max2 = resolve.has_value() ? resolve->attachment : 0;
				const std::uint32_t max3 = depthStencil.has_value() ? depthStencil->attachment : 0;
				const std::uint32_t max4 = std::ranges::max(color, std::less<std::uint32_t>{}, &VkAttachmentReference::attachment).attachment;

				return std::max({max1, max2, max3, max4});
			}
		};

		struct SubpassData{
			std::uint32_t index{};

			VkSubpassDescription description{
					.flags = 0,
					.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS
				};
			std::vector<VkSubpassDependency> dependencies{};

			AttachmentReference attachment{};

			[[nodiscard]] std::uint32_t getPrevIndex() const noexcept{
				return index - 1;
			}

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

			void addDependency(const VkSubpassDependency& dependency){
				dependencies.push_back(dependency);
				dependencies.back().dstSubpass = index;
			}

			void addAttachment(std::initializer_list<std::tuple<const AttachmentReference::Category, const std::uint32_t, const VkImageLayout>> list){
				for (const auto& [category, index, layout] : list){
					addAttachment(category, index, layout);
				}
			}

			void addInput(const std::uint32_t index){
				addAttachment(AttachmentReference::Category::input, index, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}

			void addOutput(const std::uint32_t index){
				addAttachment(AttachmentReference::Category::color, index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}

			void addReserved(const std::uint32_t index){
				addAttachment(AttachmentReference::Category::reserved, index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}

			void addAttachment(const AttachmentReference::Category attachmentCategory, const std::uint32_t index, const VkImageLayout imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL){
				VkAttachmentReference attachmentReference{index, imageLayout};

				switch(attachmentCategory){
					case AttachmentReference::Category::input:{
						if(attachment.input){
							attachment.input->push_back(attachmentReference);
						}else{
							attachment.input.emplace({attachmentReference});
						}

						return;
					}

					case AttachmentReference::Category::color:
						attachment.color.push_back(attachmentReference);
						return;

					case AttachmentReference::Category::depthStencil:
						attachment.depthStencil = attachmentReference;
						return;

					case AttachmentReference::Category::resolve:
						attachment.resolve = attachmentReference;
						return;

					case AttachmentReference::Category::reserved:{
						if(attachment.reserved){
							attachment.reserved->push_back(index);
						}else{
							attachment.reserved.emplace({index});
						}

						return;
					}

					default: std::unreachable();
				}

			}
		};

		std::vector<SubpassData> subpasses{};

		[[nodiscard]] RenderPass() = default;

		[[nodiscard]] explicit RenderPass(VkDevice device)
			: device{device}{}

		[[nodiscard]] RenderPass(VkRenderPass_T* handler, VkDevice device)
			: Wrapper{handler},
			  device{device}{}

		~RenderPass(){
			destroy();
		}

		RenderPass(const RenderPass& other) = delete;

		RenderPass(RenderPass&& other) noexcept = default;

		RenderPass& operator=(const RenderPass& other) = delete;

		RenderPass& operator=(RenderPass&& other) noexcept{
			if(this == &other) return *this;
			destroy();
			Wrapper<VkRenderPass>::operator =(std::move(other));
			device = std::move(other.device);
			attachmentSockets = std::move(other.attachmentSockets);
			subpasses = std::move(other.subpasses);
			return *this;
		}

		[[nodiscard]] auto subpassSize() const noexcept{
			return subpasses.size();
		}

		[[nodiscard]] auto attachmentsSize() const noexcept{
			return attachmentSockets.size();
		}

		void pushAttachment(const VkAttachmentDescription& description){
			attachmentSockets.push_back(description);
		}

		template <std::regular_invocable<SubpassData&> InitFunc>
		void pushSubpass(InitFunc&& initFunc){
			auto index = subpasses.size();
			SubpassData& data = subpasses.emplace_back(index);

			initFunc(data);
		}

		void destroy(){
			if(device)vkDestroyRenderPass(device, handle, nullptr);
			handle = nullptr;
		}

		void createRenderPass(){
			destroy();

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
