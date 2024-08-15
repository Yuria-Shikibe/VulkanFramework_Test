module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer.FrameBuffer;

import Core.Vulkan.Dependency;
import Core.Vulkan.Attachment;
import Core.Vulkan.Concepts;

import std;
import Geom.Vector2D;

export namespace Core::Vulkan{
	class FrameBuffer : public Wrapper<VkFramebuffer>{
	protected:
		Dependency<VkDevice> device{};

		std::vector<VkImageView> attachments{};

		Geom::USize2 size{};

	public:

		void destroy(){
			if(device && handle)vkDestroyFramebuffer(device, handle, nullptr);
			handle = nullptr;
		}

		~FrameBuffer(){
			destroy();
		}

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
			return *this;
		}

		FrameBuffer(VkDevice device, const VkFramebufferCreateInfo& createInfo)
			:
			device{device},
			attachments{createInfo.pAttachments, createInfo.pAttachments + createInfo.attachmentCount},
			size{createInfo.width, createInfo.height}{
			if(vkCreateFramebuffer(device, &createInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}

		FrameBuffer(VkDevice device, const Geom::USize2 size, VkRenderPass renderPass, ContigiousRange<VkImageView> auto&& attachments, const std::uint32_t layers = 1)
			: device{device}, attachments{std::ranges::begin(attachments), std::ranges::end(attachments)}, size{size}{
			create(renderPass, layers);
		}

	protected:
		void create(VkRenderPass renderPass, const std::uint32_t layers = 1){
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

	struct IndexedAttachment : Attachment{
		std::uint32_t index{};

		using Attachment::Attachment;

		IndexedAttachment(std::uint32_t targetIndex, Attachment&& attachment) :
			Attachment{std::move(attachment)}, index{targetIndex}{
		}

		IndexedAttachment(const IndexedAttachment& other) = delete;

		IndexedAttachment(IndexedAttachment&& other) noexcept = default;

		IndexedAttachment& operator=(const IndexedAttachment& other) = delete;

		IndexedAttachment& operator=(IndexedAttachment&& other) noexcept = default;
	};

	struct FramebufferLocal : FrameBuffer{
		using FrameBuffer::FrameBuffer;

		void pushCapturedAttachments(Attachment&& attachment){
			localAttachments.push_back(IndexedAttachment{static_cast<std::uint32_t>(localAttachments.size()), std::move(attachment)});
		}

		std::vector<IndexedAttachment> localAttachments{};

		void setDevice(VkDevice device){
			this->device = device;
		}

		void setSize(const Geom::USize2 size){
			this->size = size;
		}

		void loadCapturedAttachments(const std::size_t size){
			if(size){
				attachments.resize(size);
			}else{
				const std::uint32_t max = std::ranges::max(localAttachments | std::views::transform(&IndexedAttachment::index));
				attachments.resize(max);
			}


			for (const auto & attachment : localAttachments){
				attachments[attachment.index] = attachment.getView();
			}
		}

		template <std::ranges::range Range>
			requires (std::same_as<std::ranges::range_value_t<Range>, std::pair<std::uint32_t, VkImageView>>)
		void loadExternalImageView(Range&& range){
			for(const std::pair<std::uint32_t, VkImageView> pair : range){
				if((pair.first + 1) > attachments.size()){
					attachments.resize((pair.first + 1));
				}

				attachments[pair.first] = pair.second;
			}
		}


		void loadExternalImageView(const std::initializer_list<std::pair<std::uint32_t, VkImageView>> list){
			for(const std::pair<std::uint32_t, VkImageView> pair : list){
				if((pair.first + 1) > attachments.size()){
					attachments.resize((pair.first + 1));
				}

				attachments[pair.first] = pair.second;
			}
		}

		void checkAttachments() noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
			if(std::ranges::any_of(attachments, [](VkImageView attachment){return attachment == nullptr;})){
				throw std::runtime_error{"Attachment Cannot be VK_NULL_HANDLE"};
			}
#endif
		}

		void createFrameBuffer(VkRenderPass renderPass){
			checkAttachments();

			if(!renderPass){
				throw std::runtime_error{"Invalid RenderPass"};
			}

			create(renderPass);
		}

	};
}
