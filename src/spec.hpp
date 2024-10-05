#pragma once

#ifdef _MSC_VER
#define SPEC_WEAK /*__declspec(selectany)*/
#else
#define SPEC_WEAK __attribute__((weak))
#endif