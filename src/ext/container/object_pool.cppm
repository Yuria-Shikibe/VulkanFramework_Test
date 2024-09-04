export module ext.object_pool;

import ext.array_stack;
import std;

namespace ext{
	template <typename T, std::size_t count>
	struct object_pool_page{
		T* data;
		mutable std::mutex mutex{};

		array_stack<T*, count> valid_pointers{};

		[[nodiscard]] object_pool_page() = default;

		[[nodiscard]] constexpr explicit object_pool_page(T* data) : data{data}{
			std::scoped_lock lk{mutex};

			[this]<std::size_t ...I>(std::index_sequence<I...>){
				(valid_pointers.push(this->data + I), ...);
			}(std::make_index_sequence<count>{});
		}

		/** @brief DEBUG USAGE */
		[[nodiscard]] constexpr std::span<T> as_span() const noexcept{
			return std::span{data, count};
		}

		template <std::invocable<object_pool_page&> Func>
		decltype(auto) lock_and(Func&& func){
			std::scoped_lock lk{mutex};
			return func(*this);
		}

		template<typename Alloc>
		constexpr void free(Alloc& alloc) const noexcept(!DEBUG_CHECK){
			std::scoped_lock lk{mutex};

#if DEBUG_CHECK
			if(!valid_pointers.full()){
				throw std::runtime_error("pool_page::free: not all pointers are stored");
			}
#endif

			std::allocator_traits<Alloc>::deallocate(alloc, data, count);
		}

		constexpr void store(T* p) noexcept(!DEBUG_CHECK && noexcept(noexcept(p->~T()))){
			p->~T();

			std::scoped_lock lk{mutex};
			valid_pointers.push(p);

#if DEBUG_CHECK
			if(valid_pointers.size() > count){
				throw std::runtime_error("pool_page::store: pointers overflow");
			}
#endif
		}

		/**
		 * @return nullptr if the page is empty
		 */
		template <typename... Args>
		constexpr T* borrow(Args&&... args) noexcept(noexcept(T{std::forward<Args>(args)...})){
			T* p;

			{
				std::scoped_lock lk{mutex};

				if(valid_pointers.empty()){
					return nullptr;
				}

				p = valid_pointers.top();
				valid_pointers.pop();
			}

			new (p) T{std::forward<Args>(args)...};
			return p;
		}

		object_pool_page(const object_pool_page& other) = delete;

		constexpr object_pool_page(object_pool_page&& other) noexcept {
			std::scoped_lock lk{mutex, other.mutex};
			data = other.data;
			valid_pointers = std::move(other.valid_pointers);
		}

		object_pool_page& operator=(const object_pool_page& other) = delete;

		constexpr object_pool_page& operator=(object_pool_page&& other) noexcept{
			if(this == &other) return *this;
			std::scoped_lock lk{mutex, other.mutex};
			data = other.data;
			valid_pointers = std::move(other.valid_pointers);
			return *this;
		}
	};

	template <typename T, std::size_t count>
	struct pool_deleter{
		object_pool_page<T, count>* page{};

		[[nodiscard]] pool_deleter() = default;

		[[nodiscard]] explicit pool_deleter(object_pool_page<T, count>* page)
			: page{page}{}

		void operator()(T* ptr) const{
			page->store(ptr);
		}
	};

	export
	template <typename T, std::size_t PageSize = 1000, typename Alloc = std::allocator<T>>
	struct object_pool{
		using page_type = object_pool_page<T, PageSize>;
		using deleter_type = pool_deleter<T, PageSize>;
		using unique_ptr = std::unique_ptr<T, deleter_type>;
		using allocator_type = Alloc;

	private:
		allocator_type allocator{};
		mutable std::shared_mutex mutex{};
		std::list<page_type> pages{};

	public:

		template <typename... Args>
		[[nodiscard]] unique_ptr obtain_unique(Args&&... args) noexcept(noexcept(T{std::forward<Args>(args)...})) {
			auto [ptr, page] = this->obtain_raw(std::forward<Args>(args)...);
			return unique_ptr{ptr, deleter_type{page}};
		}

		template <typename... Args>
		[[nodiscard]] std::shared_ptr<T> obtain_shared(Args&&... args) noexcept(noexcept(T{std::forward<Args>(args)...})) {
			auto [ptr, page] = this->template obtain_raw<Args...>(std::forward<Args>(args)...);
			return std::shared_ptr<T>{ptr, deleter_type{page}};
		}

		template <typename... Args>
		[[nodiscard]] std::tuple<T*, page_type*> obtain_raw(Args&&... args) noexcept(!DEBUG_CHECK && noexcept(T{std::forward<Args>(args)...})){
			T* rst{};

			{
				std::shared_lock lk{mutex};

				for (page_type& page : pages){
					rst = page.borrow(std::forward<Args>(args)...);
					if(rst)return {rst, &page};
				}
			}

			page_type& nextPage = add_page();
			rst = nextPage.borrow(std::forward<Args>(args)...);

#if DEBUG_CHECK
			if(!rst){
				throw std::runtime_error("Failed To Obtain Object!");
			}
#endif

			return {rst, &nextPage};
		}

		/** @brief DEBUG USAGE */
		[[nodiscard]] const std::list<page_type>& get_pages() const noexcept{
			return pages;
		}

		page_type& add_page(){
			std::unique_lock lk{mutex};

			T* ptr = std::allocator_traits<allocator_type>::allocate(allocator, PageSize);
			page_type& page = pages.emplace_back(ptr);
			return page;
		}

		void free_page(typename std::list<page_type>::const_iterator& itr){
			std::unique_lock lk{mutex};

			itr->free(allocator);
			pages.erase(itr);
		}

		void shrink(){
			std::unique_lock lk{mutex};

			std::erase_if(pages, [this](page_type& page){
				return page.lock_and([this](page_type& p){
					if(p.valid_pointers.full()){
						p.free(allocator);
						return true;
					}else{
						return false;
					}
				});
			});
		}

		[[nodiscard]] object_pool() = default;

		~object_pool(){
			std::unique_lock lk{mutex};

			for (const auto & page : pages){
				page.free(allocator);
			}
		}

		object_pool(const object_pool& other) = delete;

		object_pool(object_pool&& other) noexcept{
			std::scoped_lock lk{mutex, other.mutex};

			for (const auto & page : pages){
				page.free(allocator);
			}

			allocator = std::move(other.allocator);
			pages = std::move(other.pages);
		}

		object_pool& operator=(const object_pool& other) = delete;

		object_pool& operator=(object_pool&& other) noexcept{
			if(this == &other) return *this;
			std::scoped_lock lk{mutex, other.mutex};

			for (const auto & page : pages){
				page.free(allocator);
			}

			allocator = std::move(other.allocator);
			pages = std::move(other.pages);
			return *this;
		}
	};
}
