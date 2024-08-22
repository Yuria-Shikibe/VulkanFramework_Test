#pragma once

#define LoadFuncPtr(instance, name) \
reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name))
