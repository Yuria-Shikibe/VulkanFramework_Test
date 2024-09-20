//
// Created by Matrix on 2024/8/26.
//

#ifndef ENUM_OPERATOR_GEN_HPP
#define ENUM_OPERATOR_GEN_HPP

#define BITMASK_OPS_BASE(EXPORT_FLAG, BITMASK)\
EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator|(BITMASK lhs, BITMASK rhs) noexcept {\
return static_cast<BITMASK>(std::to_underlying(lhs) | std::to_underlying(rhs));       \
}                                                                                         \
\
EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator^(BITMASK lhs, BITMASK rhs) noexcept {\
return static_cast<BITMASK>(std::to_underlying(lhs) ^ std::to_underlying(rhs));       \
}                                                                                         \
\
EXPORT_FLAG constexpr BITMASK& operator|=(BITMASK& lhs, BITMASK rhs) noexcept {         \
return lhs = lhs | rhs;                                                               \
}                                                                                         \
\
EXPORT_FLAG constexpr BITMASK& operator^=(BITMASK& lhs, BITMASK rhs) noexcept {         \
return lhs = lhs ^ rhs;                                                               \
}                                                                                         \
\
EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator~(BITMASK lhs) noexcept {             \
return static_cast<BITMASK>(~std::to_underlying(lhs));                                \
}\


#define BITMASK_OPS(EXPORT_FLAG, BITMASK)                                                     \
    EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator&(BITMASK lhs, BITMASK rhs) noexcept {\
        return static_cast<BITMASK>(std::to_underlying(lhs) & std::to_underlying(rhs));       \
    }                                                                                         \
\
EXPORT_FLAG constexpr BITMASK& operator&=(BITMASK& lhs, BITMASK rhs) noexcept {         \
        return lhs = lhs & rhs;                                                               \
    }\
BITMASK_OPS_BASE(EXPORT_FLAG, BITMASK)\

#define BITMASK_OPS_BOOL(EXPORT_FLAG, BITMASK)                                                     \
    EXPORT_FLAG [[nodiscard]] constexpr auto operator&(BITMASK lhs, BITMASK rhs) noexcept {\
        return static_cast<bool>(std::to_underlying(lhs) & std::to_underlying(rhs));       \
    }                                                                                         \
\
BITMASK_OPS_BASE(EXPORT_FLAG, BITMASK)\



#define BITMASK_OPS_ADDITIONAL(EXPORT_FLAG, BITMASK)                                          \
EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator-(BITMASK lhs, BITMASK rhs) noexcept {\
return static_cast<BITMASK>(std::to_underlying(lhs) & ~std::to_underlying(rhs));      \
}                                                                                         \
\
\
EXPORT_FLAG constexpr BITMASK& operator-=(BITMASK& lhs, BITMASK rhs) noexcept {           \
return lhs = lhs - rhs;                                                               \
}

#endif //ENUM_OPERATOR_GEN_HPP
