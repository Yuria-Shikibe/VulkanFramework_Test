module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.BatchData;

export namespace Core::Vulkan{
	struct BatchData{
		VkBuffer vertices{};
		VkBuffer indices{};
		VkIndexType indexType{};
		VkBuffer indirect{};

		void bindTo(VkCommandBuffer commandBuffer, const VkDeviceSize offset = 0) const{
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices, &offset);
			vkCmdBindIndexBuffer(commandBuffer, indices, 0, indexType);
			vkCmdDrawIndexedIndirect(commandBuffer, indirect, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
		}
	};
}
