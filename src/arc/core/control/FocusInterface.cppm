//
// Created by Matrix on 2024/5/12.
//

export module Core.Ctrl:FocusInterface;

import ext.RuntimeException;
import ext.concepts;
import ext.meta_programming;

import std;

export namespace Core::Ctrl {
    template <typename T>
    struct FocusData {
        T current;
        T fallback;
    };

    template <typename T>
    struct FocusInterface {
        static constexpr bool scalar = std::is_scalar_v<T>;

        using PassType = typename ext::ConstConditional<!scalar, typename ext::conditional_reference<!scalar, T>::type>::type;

        static bool defValidCheck(PassType t) {
            return static_cast<bool>(t);
        }

        template <bool(*checker)(PassType t) = FocusInterface::defValidCheck>
        struct Type : FocusData<T> {
            static constexpr bool (*validChecker)(PassType t) = checker;

            using FocusData<T>::current;
            using FocusData<T>::fallback;

            void check() {
                if(!validChecker(current)) [[unlikely]] {
                    if(!validChecker(fallback)) [[unlikely]] {
                        throw ext::RuntimeException{"Invalid Focus State!"};
                    }

                    current = fallback;
                }
            }
        };
    };

    template <typename T>
    using BasicFocusInterface = typename FocusInterface<T>::template Type<>;
}
