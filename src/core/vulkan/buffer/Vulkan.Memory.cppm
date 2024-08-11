module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Memory;
import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	std::uint32_t findMemoryType(const std::uint32_t typeFilter, VkPhysicalDevice physicalDevice,
							 VkMemoryPropertyFlags properties){
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for(std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
			if(typeFilter & (1u << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties){

				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	class DeviceMemory : public Wrapper<VkDeviceMemory>{
	public:

	protected:
		Dependency<VkDevice> device{};

		VkDeviceSize capacity{};

		VkMemoryPropertyFlags properties{};
		VkDeviceSize nonCoherentAtomSize{};

	public:
		[[nodiscard]] DeviceMemory() = default;

		[[nodiscard]] explicit DeviceMemory(VkDevice device)
			: device{device}{}

		[[nodiscard]] explicit DeviceMemory(VkDevice device, VkMemoryPropertyFlags properties)
			: device{device}, properties{properties}{}

		[[nodiscard]] constexpr VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] constexpr std::size_t size() const noexcept{ return capacity; }

		~DeviceMemory(){
			deallocate();
		}

		DeviceMemory(const DeviceMemory& other) = delete;

		DeviceMemory(DeviceMemory&& other) noexcept = default;

		DeviceMemory& operator=(const DeviceMemory& other) = delete;

		DeviceMemory& operator=(DeviceMemory&& other) noexcept{
			if(this == &other) return *this;
			deallocate();
			Wrapper::operator =(std::move(other));
			device = std::move(other.device);
			capacity = other.capacity;
			properties = other.properties;
			nonCoherentAtomSize = other.nonCoherentAtomSize;
			return *this;
		}

		template <std::ranges::contiguous_range T>
			requires (std::ranges::sized_range<T>)
		void loadData(const T& range) const{
			const std::size_t dataSize = std::ranges::size(range) * sizeof(std::ranges::range_value_t<T>);

			this->loadData(std::ranges::data(range), dataSize);
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T& data) const{
			static constexpr std::size_t dataSize = sizeof(T);

			this->loadData(&data, dataSize);
		}

		template <typename T, std::size_t size>
		void loadData(const std::array<T, size>& data) const{
			this->loadData(&data, size);
		}

		template <typename T, typename... Args>
		void emplace(Args&&... args) const{
			static constexpr std::size_t dataSize = sizeof(T);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			new(pdata) T(std::forward<Args>(args)...);
			unmap();
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T* data) const{
			this->loadData(*data);
		}


		void loadData(const void* src, const std::size_t size) const{
			if(size > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			std::memcpy(pdata, src, size);
			unmap();
		}


		[[nodiscard]] bool isCoherent() const noexcept{
			return properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		}

		void unmap_noFlush() const{
			vkUnmapMemory(device, handle);
		}

		void unmap(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const{
			if(!isCoherent()){
				flush(size, offset);
			}

			unmap_noFlush();
		}

		void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const{
			adjustNonCoherentMemoryRange(size, offset);
			const VkMappedMemoryRange mappedMemoryRange = {
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = handle,
				.offset = offset,
				.size = size
			};

			if(vkFlushMappedMemoryRanges(device, 1, &mappedMemoryRange)){
				throw std::runtime_error("Failed to unmap memory!");
			}
		}

		[[nodiscard]] void* map_noInvalidation(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const{
			VkDeviceSize inverseDeltaOffset{};
			void* pData{};

			if(!isCoherent()){
				 inverseDeltaOffset = adjustNonCoherentMemoryRange(size, offset);
			}

			if(vkMapMemory(device, handle, offset, size, 0, &pData)){
				throw std::runtime_error("Failed to map memory!");
			}

			if(!isCoherent()){
				pData = static_cast<std::uint8_t*>(pData) + inverseDeltaOffset;
			}

			return pData;
		}

		[[nodiscard]] void* map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const{
			void* pData = map_noInvalidation(size, offset);

			if(!isCoherent()){
				invalidate(size, offset);
			}

			return pData;
		}

		void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const{
			(void)adjustNonCoherentMemoryRange(size, offset);

			const VkMappedMemoryRange mappedMemoryRange{
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = handle,
				.offset = offset,
				.size = size
			};

			if(vkInvalidateMappedMemoryRanges(device, 1, &mappedMemoryRange)){
				throw std::runtime_error("Failed to map memory!");
			}
		}

		VkResult allocate(VkPhysicalDevice physicalDevice, VkBuffer buffer, VkDeviceSize size) {
			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

			capacity = size;
			return allocate(physicalDevice, memRequirements);
		}

		VkResult allocate(VkPhysicalDevice physicalDevice, VkImage image) {
			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(device, image, &memRequirements);

			capacity = memRequirements.size;

			return allocate(physicalDevice, memRequirements);
		}

		VkResult allocate(VkPhysicalDevice physicalDevice, const VkMemoryRequirements& memRequirements) {
			VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, physicalDevice, properties);

			if(device && handle)vkFreeMemory(device, handle,nullptr);

			if (const VkResult result = vkAllocateMemory(device, &allocInfo, nullptr, &handle)) {
				return result;
			}

			return VK_SUCCESS;
		}

		void deallocate(){
			if(device && handle)vkFreeMemory(device, handle,nullptr);
			handle = nullptr;
		}

		void acquireLimit(VkPhysicalDevice physicalDevice){
			if(!nonCoherentAtomSize && !isCoherent()){
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

				nonCoherentAtomSize = deviceProperties.limits.nonCoherentAtomSize;
			}else{
				nonCoherentAtomSize = 1;
			}
		}

	protected:

		constexpr VkDeviceSize adjustNonCoherentMemoryRange(VkDeviceSize& size, VkDeviceSize& offset) const {
			const VkDeviceSize _offset = offset;

			if(size == VK_WHOLE_SIZE){
				offset = offset / nonCoherentAtomSize * nonCoherentAtomSize;
			}else{
				VkDeviceSize rangeEnd = size + offset;

				offset = offset / nonCoherentAtomSize * nonCoherentAtomSize;

				rangeEnd = (rangeEnd + nonCoherentAtomSize - 1) / nonCoherentAtomSize * nonCoherentAtomSize;

				rangeEnd = std::min(rangeEnd, capacity);

				size = rangeEnd - offset;
			}

			return _offset - offset;
		}
	};
}