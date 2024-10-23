module;

#include "../adapted_attributes.hpp"

export module ext.shared_stack;

import std;

namespace ext{
	//TODO safe destructon on throw

	// using T = int;
	// using Alloc = std::allocator<T>;
	template <typename T, typename Alloc>
	struct shared_array_stack_base{
		ADAPTED_NO_UNIQUE_ADDRESS
		Alloc alloc{};

	protected:
		std::size_t cap{};
		T* data{};

	public:
		[[nodiscard]] shared_array_stack_base() = default;

		[[nodiscard]] explicit shared_array_stack_base(const std::size_t size, const Alloc& alloc = {}) :
			alloc{alloc},
			cap{size},
			data(std::allocator_traits<Alloc>::allocate(this->alloc, size)){
		}

		~shared_array_stack_base(){
			free();
		}

	protected:
		void free() noexcept{
			if(data)std::allocator_traits<Alloc>::deallocate(this->alloc, data, cap);
			data = nullptr;
		}

		void allocate(const std::size_t size) noexcept{
			cap = size;
			data = std::allocator_traits<Alloc>::allocate(this->alloc, size);
		}
	};

	//TODO using rls - aqr instead of this bullshit?
	export
	template <typename T, typename Alloc = std::allocator<T>>
	struct shared_stack : shared_array_stack_base<T, Alloc>{
	private:
		std::atomic_size_t top{};

	public:
		[[nodiscard]] shared_stack() = default;

		[[nodiscard]] explicit shared_stack(const std::size_t size, const Alloc& alloc = {}) :
			shared_array_stack_base<T, Alloc>{size, alloc}
		{}

		[[nodiscard]] T* push_uninitialized() noexcept{
			const auto index = top.fetch_add(1, std::memory_order_acq_rel);
			assert(index <= this->cap);
			return this->data + index;
		}

		template<typename... Args>
			requires (std::constructible_from<T, Args...>)
		T& emplace(Args&& ...args) noexcept(std::construct_at(nullptr, std::forward<Args>(args)...)){
			auto where = push_uninitialized();

			return *std::construct_at(where, std::forward<Args>(args)...);
		}

		T& push(T&& val) noexcept(std::is_nothrow_move_assignable_v<T>) requires (std::is_move_assignable_v<T>){
			auto where = push_uninitialized();

			return *std::construct_at(where, std::move(val));
		}

		T& push(const T& val) noexcept(std::is_nothrow_copy_assignable_v<T>) requires (std::is_copy_assignable_v<T>){
			auto where = push_uninitialized();

			return *std::construct_at(where, val);
		}


		[[nodiscard]] std::size_t size() const noexcept{
			return top.load(std::memory_order_acquire);
		}

		[[nodiscard]] T* begin() noexcept{
			return this->data;
		}

		[[nodiscard]] T* end() noexcept{
			return this->data + size();
		}

		[[nodiscard]] const T* begin() const noexcept{
			return this->data;
		}

		[[nodiscard]] const T* end() const noexcept{
			return this->data + size();
		}

		[[nodiscard]] const T* cbegin() const noexcept{
			return this->data;
		}

		[[nodiscard]] const T* cend() const noexcept{
			return this->data + size();
		}
		
		[[nodiscard]] std::size_t capacity() const noexcept{
			return this->cap;
		}

		[[nodiscard]] T& operator[](std::size_t idx) noexcept{
			assert(idx < size());

			return this->data[idx];
		}

		[[nodiscard]] const T& operator[](std::size_t idx) const noexcept{
			assert(idx < size());

			return this->data[idx];
		}

		void clear_and_reserve(const std::size_t newSize){
			destroy();
			top.store(0, std::memory_order_release);
			if(this->cap >= newSize)return;

			this->free();
			this->allocate(newSize);
		}

	private:
		void destroy(){
			if constexpr (!std::is_trivially_destructible_v<T>){
				std::destroy_n(this->data, size());
			}
		}
	public:

		~shared_stack(){
			destroy();
		}


		shared_stack(const shared_stack& other) = delete;
		shared_stack(shared_stack&& other) noexcept = delete;
		shared_stack& operator=(const shared_stack& other) = delete;
		shared_stack& operator=(shared_stack&& other) noexcept = delete;
	};

}
