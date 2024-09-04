#pragma once

#include <cassert>

#define LoadFuncPtr(instance, name) \
	[instance](){\
	auto ptr = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name));\
	assert(ptr != nullptr);\
	return ptr;\
}()\

