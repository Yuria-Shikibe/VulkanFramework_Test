module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DescriptorBuffer;

import ext.handle_wrapper;
import Core.Vulkan.Buffer.PersistentTransferBuffer;
import Core.Vulkan.Memory;

export import Core.Vulkan.EXT;

import std;

export namespace Core::Vulkan{
	//OPTM bad interface design
	template <typename T>
	struct DescriptorBufferBase{
	private:
		[[nodiscard]] T& self(){
			return *static_cast<T*>(this);
		}

		[[nodiscard]] const T& self() const{
			return *static_cast<const T*>(this);
		}

	public:
		VkDeviceSize size{};
		std::vector<VkDeviceSize> offsets{};

		std::size_t uniformBufferDescriptorSize{};
		std::size_t combinedImageSamplerDescriptorSize{};
		std::size_t storageImageDescriptorSize{};
		std::size_t inputAttachmentDescriptorSize{};

		[[nodiscard]] DescriptorBufferBase() = default;

		[[nodiscard]] DescriptorBufferBase(VkPhysicalDevice physicalDevice, VkDevice device,
			VkDescriptorSetLayout layout, std::uint32_t bindings){
			//TODO move this to other place
			VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
				.pNext = nullptr,
			};

			VkPhysicalDeviceProperties2KHR device_properties{};

			descriptor_buffer_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
			device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
			device_properties.pNext = &descriptor_buffer_properties;
			EXT::getPhysicalDeviceProperties2KHR(physicalDevice, &device_properties);

			// Get set layout descriptor sizes.
			EXT::getDescriptorSetLayoutSizeEXT(device, layout, &size);

			// Adjust set layout sizes to satisfy alignment requirements.
			size = Util::alignTo(size, descriptor_buffer_properties.descriptorBufferOffsetAlignment);

			offsets.resize(bindings);
			// Get descriptor bindings offsets as descriptors are placed inside set layout by those offsets.
			for (auto&& [i, offset] : offsets | std::views::enumerate){
				EXT::getDescriptorSetLayoutBindingOffsetEXT(device, layout, i, offsets.data() + i);
			}

			uniformBufferDescriptorSize = descriptor_buffer_properties.uniformBufferDescriptorSize;
			combinedImageSamplerDescriptorSize = descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
			storageImageDescriptorSize = descriptor_buffer_properties.storageImageDescriptorSize;
			inputAttachmentDescriptorSize = descriptor_buffer_properties.inputAttachmentDescriptorSize;

		}

		[[nodiscard]] std::pair<VkDescriptorGetInfoEXT, VkDeviceSize> getImageInfo(const VkDescriptorImageInfo& imageInfo, const VkDescriptorType descriptorType) const{
			std::pair<VkDescriptorGetInfoEXT, VkDeviceSize> rst{VkDescriptorGetInfoEXT{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
				.pNext = nullptr,
				.type = descriptorType,
				.data = {}
			}, {}};

			switch(descriptorType){
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :{
					if(!imageInfo.sampler)throw std::invalid_argument("NULL Sampler");
					rst.first.data.pCombinedImageSampler = &imageInfo;
					rst.second = combinedImageSamplerDescriptorSize;
					break;
				}

				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE :{
					rst.first.data.pStorageImage = &imageInfo;
					rst.second = storageImageDescriptorSize;
					break;
				}

				case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT :{
					rst.first.data.pInputAttachmentImage = &imageInfo;
					rst.second = inputAttachmentDescriptorSize;
					break;
				}

				default : throw std::runtime_error("Invalid descriptor type!");
			}

			return rst;
		}

		void loadUniform(const std::uint32_t binding, const VkDeviceAddress address, const VkDeviceSize size) const{
			const VkDescriptorAddressInfoEXT addr_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
				.pNext = nullptr,
				.address = address,
				.range = size,
				.format = VK_FORMAT_UNDEFINED
			};

			const VkDescriptorGetInfoEXT info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
				.pNext = nullptr,
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.data = VkDescriptorDataEXT{
					.pUniformBuffer = &addr_info
				}
			};

			EXT::getDescriptorEXT(
				self().getDevice(),
				&info,
				uniformBufferDescriptorSize,
				self().getMappedData() + offsets[binding]
			);
		}

		void loadImage(
			const std::uint32_t binding,
			const VkDescriptorImageInfo& imageInfo,
			const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) const{

			auto [info, size] = getImageInfo(imageInfo, descriptorType);

			EXT::getDescriptorEXT(
				self().getDevice(),
				&info,
				size,
				self().getMappedData() + offsets[binding]
			);
		}

		template <std::ranges::input_range Rng>
			requires (std::convertible_to<std::ranges::range_const_reference_t<Rng>, const VkDescriptorImageInfo&>)
		void loadImage(
			const std::uint32_t binding,
			const Rng& imageInfos,
			const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) const{

			std::size_t currentOffset = 0;
			for(const VkDescriptorImageInfo& imageInfo : imageInfos){
				auto [info, size] = getImageInfo(imageInfo, descriptorType);

				EXT::getDescriptorEXT(
					self().getDevice(),
					&info,
					size,
					self().getMappedData() + offsets[binding] + currentOffset
				);

				currentOffset += size;
			}
		}

		void loadImage(
			const std::uint32_t binding,
			VkImageView imageView, const VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkSampler sampler = nullptr,
			VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			) const{
			const VkDescriptorImageInfo image_descriptor{
				.sampler = sampler,
				.imageView = imageView,
				.imageLayout = imageLayout
			};

			loadImage(binding, image_descriptor, descriptorType);
		}

		template <std::invocable<T&> Writer>
		void load(Writer&& writer) {
			self().map();
			writer(self());
			self().unmap();
		}

		[[nodiscard]] VkDescriptorBufferBindingInfoEXT getBindInfo(const VkBufferUsageFlags usage) const{
			return {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
				.pNext = nullptr,
				.address = self().getBufferAddress(),
				.usage = usage
			};
		}

		void bindTo(VkCommandBuffer commandBuffer, const VkBufferUsageFlags usage) const{
			const auto info = getBindInfo(usage);
			EXT::cmdBindDescriptorBuffersEXT(commandBuffer, 1, &info);
		}
	};


	struct DescriptorBuffer : DescriptorBufferBase<DescriptorBuffer>, PersistentTransferBuffer{
		[[nodiscard]] DescriptorBuffer() = default;

		[[nodiscard]] DescriptorBuffer(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDescriptorSetLayout layout, std::uint32_t bindings
		) : DescriptorBufferBase{physicalDevice, device, layout, bindings}{
			memory = DeviceMemory{device,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

			VkBufferCreateInfo bufferInfo{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.size = size,
				.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			};

			if(vkCreateBuffer(device, &bufferInfo, nullptr, &handle) != VK_SUCCESS){
				throw std::runtime_error("Failed to create buffer!");
			}

			memory.acquireLimit(physicalDevice);

			memory.allocate(physicalDevice, handle, size);

			vkBindBufferMemory(device, handle, memory, 0);
			setAddress();
		}
	};

	struct DescriptorBuffer_Double : DescriptorBufferBase<DescriptorBuffer_Double>, PersistentTransferDoubleBuffer{
		[[nodiscard]] DescriptorBuffer_Double() = default;

		[[nodiscard]] DescriptorBuffer_Double(
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDescriptorSetLayout layout, const std::uint32_t bindings
		) : DescriptorBufferBase{physicalDevice, device, layout, bindings},
		PersistentTransferDoubleBuffer{physicalDevice, device, this->size, VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT}{
			targetBuffer.setAddress();
		}
	};
}
