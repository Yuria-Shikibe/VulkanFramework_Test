//
// Created by Matrix on 2024/8/26.
//

#ifndef ENUM_OPERATOR_GEN_HPP
#define ENUM_OPERATOR_GEN_HPP

#define BITMASK_OPS(EXPORT_FLAG, BITMASK)                                                         \
    EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator&(BITMASK lhs, BITMASK rhs) noexcept { \
        using Ty = std:: underlying_type_t<BITMASK>;                                              \
        return static_cast<BITMASK>(static_cast<Ty>(lhs) & static_cast<Ty>(rhs));       \
    }                                                                                                 \
                                                                                                      \
    EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator|(BITMASK lhs, BITMASK rhs) noexcept { \
        using Ty = std:: underlying_type_t<BITMASK>;                                              \
        return static_cast<BITMASK>(static_cast<Ty>(lhs) | static_cast<Ty>(rhs));       \
    }                                                                                                 \
                                                                                                      \
    EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator^(BITMASK lhs, BITMASK rhs) noexcept { \
        using Ty = std:: underlying_type_t<BITMASK>;                                              \
        return static_cast<BITMASK>(static_cast<Ty>(lhs) ^ static_cast<Ty>(rhs));       \
    }                                                                                                 \
                                                                                                      \
    EXPORT_FLAG constexpr BITMASK& operator&=(BITMASK& lhs, BITMASK rhs) noexcept {         \
        return lhs = lhs & rhs;                                                                \
    }                                                                                                 \
                                                                                                      \
    EXPORT_FLAG constexpr BITMASK& operator|=(BITMASK& lhs, BITMASK rhs) noexcept {         \
        return lhs = lhs | rhs;                                                                \
    }                                                                                                 \
                                                                                                      \
    EXPORT_FLAG constexpr BITMASK& operator^=(BITMASK& lhs, BITMASK rhs) noexcept {         \
        return lhs = lhs ^ rhs;                                                                \
    }                                                                                                 \
                                                                                                      \
    EXPORT_FLAG [[nodiscard]] constexpr BITMASK operator~(BITMASK lhs) noexcept {                  \
        using Ty = std:: underlying_type_t<BITMASK>;                                              \
        return static_cast<BITMASK>(~static_cast<Ty>(lhs));                                    \
    }

#endif //ENUM_OPERATOR_GEN_HPP
