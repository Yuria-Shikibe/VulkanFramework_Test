#pragma once

#include <cassert>

#ifdef _MSC_VER

#define ADAPTED_ASSUME(no_side_effect_expr)\
	assert(no_side_effect_expr);\
	__assume(no_side_effect_expr);\

#else
#define ADAPTED_ASSUME(no_side_effect_expr) \
	assert(no_side_effect_expr);\
	[[assume(no_side_effect_expr)]];\

#endif

