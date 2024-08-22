export module ext.Container.ArrayQueue;

import std;

export namespace ext {
    template <typename T, std::size_t capacity>
        requires (std::is_default_constructible_v<T>)
    class array_queue {
    public:
        static constexpr std::size_t MaxSize{capacity};

        [[nodiscard]] constexpr array_queue(){}

        constexpr void push(const T &item) /*noexcept(std::is_nothrow_copy_assignable_v<T>)*/ requires (std::is_copy_assignable_v<T>){
            if(is_full()) {
                throw std::overflow_error("Queue is full");
            }

            data[backIndex] = item;
            backIndex = (backIndex + 1) % MaxSize;
            ++count;
        }

        constexpr void push(T&& item) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ requires (std::is_move_assignable_v<T>){
            if(is_full()) {
                throw std::overflow_error("Queue is full");
            }

            data[backIndex] = std::move(item);
            backIndex = (backIndex + 1) % MaxSize;
            ++count;
        }

        constexpr void pop() {
            if(empty()) {
                throw std::underflow_error("Queue is empty");
            }

            frontIndex = (frontIndex + 1) % MaxSize;
            --count;
        }

        [[nodiscard]] constexpr decltype(auto) front() const {
            if(empty()) {
                throw std::underflow_error("Queue is empty");
            }

            return data[frontIndex];
        }

        [[nodiscard]] constexpr decltype(auto) front() {
            if(empty()) {
                throw std::underflow_error("Queue is empty");
            }

            return data[frontIndex];
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return count == 0;
        }

        [[nodiscard]] constexpr bool is_full() const noexcept {
            return count == MaxSize;
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept {
            return count;
        }

    private:
        std::array<T, capacity> data{};

        std::size_t frontIndex{};
        std::size_t backIndex{};
        std::size_t count{};
    };
}
