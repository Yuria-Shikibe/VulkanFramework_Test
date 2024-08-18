module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.FrameBuffer;

import Core.Vulkan.Dependency;
import Core.Vulkan.Attachment;
import Core.Vulkan.Concepts;

import std;
import Geom.Vector2D;

export namespace Core::Vulkan{
	using AttachmentPluginPair = std::pair<std::uint32_t, VkImageView>;

	class FrameBuffer : public Wrapper<VkFramebuffer>{
	protected:
		Dependency<VkDevice> device{};

		std::vector<VkImageView> attachments{};

		Geom::USize2 size{};

		Dependency<VkRenderPass> renderPass{};

	public:
		[[nodiscard]] VkDescriptorImageInfo getInputInfo(const std::size_t index, VkImageLayout imageLayout) const{
			 return {
				.sampler = nullptr,
				.imageView = at(index),
				.imageLayout = imageLayout
			};
		}

		void setSize(const Geom::USize2 size){
			this->size = size;
		}

		void setRenderPass(VkRenderPass renderPass){
			this->renderPass = renderPass;
		}

		void destroy(){
			if(device && handle)vkDestroyFramebuffer(device, handle, nullptr);
			handle = nullptr;
		}

		~FrameBuffer(){
			destroy();
		}

		[[nodiscard]] FrameBuffer(VkDevice device, Geom::USize2 size, VkRenderPass renderPass)
			: device{device},
			  size{size},
			  renderPass{renderPass}{}

		[[nodiscard]] FrameBuffer() = default;

		FrameBuffer(const FrameBuffer& other) = delete;

		FrameBuffer(FrameBuffer&& other) noexcept = default;

		FrameBuffer& operator=(const FrameBuffer& other) = delete;

		FrameBuffer& operator=(FrameBuffer&& other) noexcept{
			if(this == &other) return *this;
			destroy();

			Wrapper<VkFramebuffer>::operator =(std::move(other));
			device = std::move(other.device);
			attachments = std::move(other.attachments);
			size = std::move(other.size);
			renderPass = std::move(other.renderPass);
			return *this;
		}

		FrameBuffer(VkDevice device, const VkFramebufferCreateInfo& createInfo)
			:
			device{device},
			attachments{createInfo.pAttachments, createInfo.pAttachments + createInfo.attachmentCount},
			size{createInfo.width, createInfo.height}, renderPass{createInfo.renderPass}{
			if(vkCreateFramebuffer(device, &createInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}

		FrameBuffer(VkDevice device, const Geom::USize2 size, VkRenderPass renderPass, ContigiousRange<VkImageView> auto&& attachments, const std::uint32_t layers = 1)
			: device{device}, attachments{std::ranges::begin(attachments), std::ranges::end(attachments)}, size{size}, renderPass{renderPass}{
			create(layers);
		}

		[[nodiscard]] VkImageView at(const std::size_t index) const {
			return attachments[index];
		}

	protected:
		void create(const std::uint32_t layers){
			destroy();

			if(size.area() == 0){
				throw std::invalid_argument("Invalid Size");
			}

			const VkFramebufferCreateInfo framebufferInfo{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.renderPass = renderPass,
				.attachmentCount = static_cast<std::uint32_t>(std::ranges::size(attachments)),
				.pAttachments = std::ranges::data(attachments),
				.width = size.x,
				.height = size.y,
				.layers = layers
			};

			if(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	};

	struct FramebufferLocal : FrameBuffer{
		using FrameBuffer::FrameBuffer;


		/**
		 * @warning Slice Caution: Frame Buffer Only Accepts Attachments
		 */
		void pushCapturedAttachments(Attachment&& attachment){
			attachment.index = localAttachments.size();
			localAttachments.push_back(std::move(attachment));
		}

		/**
		 * @warning Slice Caution: Frame Buffer Only Accepts Attachments
		 */
		template <std::ranges::range Rng = std::initializer_list<Attachment&&>>
		void pushCapturedAttachments(Rng&& attachments){
			for(auto&& attachment : attachments){
				attachment.index = localAttachments.size();
				localAttachments.push_back(std::move(attachment));
			}
		}

		void addCapturedAttachments(std::uint32_t index, Attachment&& attachment){
			attachment.index = index;
			localAttachments.push_back(std::move(attachment));
		}

		std::vector<Attachment> localAttachments{};

		void resize(Geom::USize2 size, VkCommandBuffer commandBuffer, const std::initializer_list<AttachmentPluginPair> list = {}){
			if(this->size == size)return;
			setSize(size);

			for (auto& localAttachment : localAttachments){
				localAttachment.resize(size, commandBuffer);
			}

			loadCapturedAttachments(attachments.size());
			loadExternalImageView(list);
			create();
		}

		template <std::ranges::range ...Range>
			requires (std::convertible_to<std::ranges::range_value_t<Range>, AttachmentPluginPair> && ...)
		void resize(Geom::USize2 size, VkCommandBuffer commandBuffer, Range&& ...range){
			if(this->size == size)return;
			setSize(size);

			for (auto& localAttachment : localAttachments){
				localAttachment.resize(size, commandBuffer);
			}

			loadCapturedAttachments(attachments.size());
			[&, t = this]<std::size_t ...I>(std::index_sequence<I...>){
				(t->loadExternalImageView<Range>(range), ...);
			}(std::index_sequence_for<Range...>{});
			create();
		}

		void loadCapturedAttachments(const std::size_t size){
			if(size){
				attachments.resize(size);
			}else if(!localAttachments.empty()){
				const std::uint32_t max = std::ranges::max(localAttachments | std::views::transform(&Attachment::index));
				attachments.resize(max + 1);
			}

			for (const auto & attachment : localAttachments){
				attachments[attachment.index] = attachment.getView();
			}
		}

		template <std::ranges::range Range>
			requires (std::convertible_to<std::ranges::range_value_t<Range>, AttachmentPluginPair>)
		void loadExternalImageView(Range&& range){
			for(const AttachmentPluginPair& pair : range){
				if((pair.first + 1) > attachments.size()){
					attachments.resize((pair.first + 1));
				}

				attachments[pair.first] = pair.second;
			}
		}

		void loadExternalImageView(const std::initializer_list<AttachmentPluginPair> list){
			for(const AttachmentPluginPair pair : list){
				if((pair.first + 1) > attachments.size()){
					attachments.resize((pair.first + 1));
				}

				attachments[pair.first] = pair.second;
			}
		}

		decltype(auto) atLocal(const std::size_t index){
			return localAttachments[index];
		}

		void create(){
			checkAttachments();

			if(!renderPass){
				throw std::runtime_error{"Invalid RenderPass"};
			}

			FrameBuffer::create(1);
		}

	protected:
		void checkAttachments() noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
			if(std::ranges::any_of(attachments, [](VkImageView attachment){return attachment == nullptr;})){
				throw std::runtime_error{"Attachment Cannot be VK_NULL_HANDLE"};
			}
#endif
		}
	};
}
