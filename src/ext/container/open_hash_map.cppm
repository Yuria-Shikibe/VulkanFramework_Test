// © 2017-2020 Erik Rigtorp <erik@rigtorp.se>
// SPDX-License-Identifier: MIT

/*
HashMap

A high performance hash map. Uses open addressing with linear
probing.

Advantages:
  - Predictable performance. Doesn't use the allocator unless load factor
    grows beyond 50%. Linear probing ensures cash efficency.
  - Deletes items by rearranging items and marking slots as empty instead of
    marking items as deleted. This is keeps performance high when there
    is a high rate of churn (many paired inserts and deletes) since otherwise
    most slots would be marked deleted and probing would end up scanning
    most of the table.

Disadvantages:
  - Significant performance degradation at high load factors.
  - Maximum load factor hard coded to 50%, memory inefficient.
  - Memory is not reclaimed on erase.
 */

module;

#include <cassert>

export module ext.open_hash_map;

import std;

namespace ext{
	template <typename T>
	concept Transparent = requires{
		typename T::is_transparent;
	};

	// using Key = std::string;
	// using Val = std::string;
	// using Hash = std::hash<Key>;
	// using KeyEqual = std::equal_to<Key>;
	// using Allocator = std::allocator<std::pair<Key, Val>>;

	export
	template <typename Key,
	          typename Val,
	          typename Hash = std::hash<Key>,
	          typename KeyEqual = std::equal_to<Key>,
	          typename Allocator = std::allocator<std::pair<Key, Val>>>
	class open_hash_map{
	public:
		using key_type = Key;
		using mapped_type = Val;
		using value_type = std::pair<key_type, mapped_type>;
		using value_type_external = std::pair<const key_type, mapped_type>;
		using size_type = std::size_t;
		using hasher = Hash;
		using key_equal = KeyEqual;
		using allocator_type = Allocator;
		using reference = value_type&;
		using const_reference = const value_type&;
		using buckets = std::vector<value_type, allocator_type>;

		static_assert(std::is_default_constructible_v<key_equal>);
		static_assert(std::is_default_constructible_v<hasher>);
		static_assert(std::is_default_constructible_v<mapped_type>);
		static_assert(std::is_copy_assignable_v<key_type>);

	private:
		static constexpr hasher Hasher{};
		static constexpr key_equal KeyEqualer{};

		template <typename K>
		static constexpr bool isHasherValid = (std::same_as<K, key_type> || Transparent<hasher>) && std::is_invocable_r_v<std::size_t, hasher, const K&>;

		template <typename K>
		static constexpr bool isEqualerValid = std::same_as<K, key_type> || Transparent<key_equal>;

		template <bool addConst>
		struct hm_iterator{
			using container_type = std::conditional_t<addConst, const open_hash_map, open_hash_map>;
			using difference_type = std::ptrdiff_t;
			using value_type = std::conditional_t<addConst, const open_hash_map::value_type, open_hash_map::value_type>;
			using mapped_type = std::conditional_t<addConst, const open_hash_map::mapped_type, open_hash_map::mapped_type>;
			using pointer = value_type*;
			using reference = std::add_lvalue_reference_t<value_type>;
			using iterator_category = std::forward_iterator_tag;

			[[nodiscard]] hm_iterator() = default;

			constexpr bool operator==(const hm_iterator& other) const noexcept = default;

			constexpr hm_iterator& operator++(){
				++idx_;
				advance_past_empty();
				return *this;
			}

			constexpr std::pair<const key_type&, mapped_type&> operator*() const noexcept{
				auto& [k, v] = hm_->buckets_[idx_];
				return {k, v};
			}

			constexpr auto operator->() const noexcept{
				return std::ranges::data(hm_->buckets_) + idx_;
			}

			constexpr explicit hm_iterator(container_type* hm) noexcept : hm_(hm){ advance_past_empty(); }

			constexpr hm_iterator(container_type* hm, const size_type idx) noexcept : hm_(hm), idx_(idx){
			}

			container_type& base(){
				return *hm_;
			}

		private:
			template <bool oAddConst>
			explicit(false) hm_iterator(const hm_iterator<oAddConst>& other)
				: hm_(other.hm_), idx_(other.idx_){
			}

			constexpr void advance_past_empty() noexcept{
				while(idx_ < hm_->buckets_.size() &&
					KeyEqualer(hm_->buckets_[idx_].first, hm_->empty_key_)){
					++idx_;
				}
			}

			container_type* hm_ = nullptr;
			typename container_type::size_type idx_ = 0;
			friend container_type;
		};

	public:
		using iterator = hm_iterator<false>;
		using const_iterator = hm_iterator<true>;

		explicit constexpr open_hash_map(
			const key_type empty_key = key_type{},
			const size_type bucket_count = 16,
			const allocator_type& alloc = allocator_type{})
			: empty_key_(empty_key), buckets_(alloc){
			buckets_.resize(std::bit_ceil(bucket_count), std::make_pair(empty_key_, Val()));
		}

		constexpr open_hash_map(const open_hash_map& other, const size_type bucket_count)
			: open_hash_map(other.empty_key_, bucket_count, other.get_allocator()){
			for(auto it = other.begin(); it != other.end(); ++it){
				insert(*it);
			}
		}

		[[nodiscard]] constexpr allocator_type get_allocator() const noexcept{
			return buckets_.get_allocator();
		}

		// Iterators
		[[nodiscard]] constexpr iterator begin() noexcept{ return iterator(this); }

		[[nodiscard]] constexpr const_iterator begin() const noexcept{ return const_iterator{this}; }

		[[nodiscard]] constexpr const_iterator cbegin() const noexcept{ return const_iterator{this}; }

		[[nodiscard]] constexpr iterator end() noexcept{ return iterator(this, buckets_.size()); }

		[[nodiscard]] constexpr const_iterator end() const noexcept{
			return const_iterator(this, buckets_.size());
		}

		[[nodiscard]] constexpr const_iterator cend() const noexcept{
			return const_iterator(this, buckets_.size());
		}

		// Capacity
		[[nodiscard]] constexpr bool empty() const noexcept{ return size() == 0; }

		[[nodiscard]] constexpr size_type size() const noexcept{ return size_; }

		[[nodiscard]] constexpr size_type max_size() const noexcept{ return buckets_.max_size() / 2; }

		// Modifiers
		constexpr void clear() noexcept{
			for(auto& b : buckets_){
				if(b.first != empty_key_){
					b.first = empty_key_;
				}
			}
			size_ = 0;
		}

		constexpr std::pair<iterator, bool> insert(const value_type& value){
			return emplace_impl(value.first, value.second);
		}

		constexpr std::pair<iterator, bool> insert(value_type&& value){
			return emplace_impl(value.first, std::move(value.second));
		}

		template <typename... Args>
		constexpr std::pair<iterator, bool> emplace(Args&&... args){
			return this->emplace_impl(std::forward<Args>(args)...);
		}

		constexpr void erase(const iterator it){ erase_impl(it); }

		constexpr size_type erase(const key_type& key){ return erase_impl(key); }

		template <typename K>
		constexpr size_type erase(const K& x){ return erase_impl(x); }

		friend constexpr void swap(open_hash_map& lhs, open_hash_map& rhs) noexcept{
			std::swap(lhs.buckets_, rhs.buckets_);
			std::swap(lhs.size_, rhs.size_);
			std::swap(lhs.empty_key_, rhs.empty_key_);
		}

		// Lookup
		constexpr mapped_type& at(const key_type& key){ return this->at_impl(key); }

		template <typename K>
		constexpr mapped_type& at(const K& x){ return this->at_impl(x); }

		[[nodiscard]] constexpr const mapped_type& at(const key_type& key) const{
			return this->at_impl(key);
		}

		template <typename K>
		[[nodiscard]] constexpr const mapped_type& at(const K& x) const{
			return this->at_impl(x);
		}

		constexpr mapped_type& operator[](const key_type& key) requires (std::is_default_constructible_v<mapped_type>){
			return this->emplace_impl(key).first->second;
		}

		[[nodiscard]] constexpr size_type count(const key_type& key) const noexcept{
			return this->count_impl(key);
		}

		template <typename K>
		[[nodiscard]] constexpr bool contains(const K& key) const noexcept{
			return this->find_impl(key) != end();
		}

		template <typename... Args>
		constexpr std::pair<iterator, bool> insert_or_assign(key_type&& key, Args&&... args){
			if(iterator itr = this->find_impl(key); itr != end()){
				itr->second = mapped_type{std::forward<Args&&>(args)...};
				return std::pair{itr, false};
			} else{
				return this->emplace_impl(std::move(key), std::forward<Args>(args)...);
			}
		}

		template <typename... Args>
		constexpr std::pair<iterator, bool> insert_or_assign(const key_type& key, Args&&... args){
			if(iterator itr = this->find_impl(key); itr != end()){
				itr->second = mapped_type{std::forward<Args&&>(args)...};
				return std::pair{itr, false};
			} else{
				return this->emplace_impl(key, std::forward<Args>(args)...);
			}
		}

		template <typename... Args>
		constexpr std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args){
			return this->emplace_impl(std::move(key), std::forward<Args>(args)...);
		}

		template <typename... Args>
		constexpr std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args){
			return this->emplace_impl(key, std::forward<Args>(args)...);
		}

		template <typename K>
		[[nodiscard]] constexpr size_type count(const K& x) const noexcept{
			return this->count_impl(x);
		}

		[[nodiscard]] constexpr iterator find(const key_type& key) noexcept{ return this->find_impl(key); }

		template <typename K>
		[[nodiscard]] constexpr iterator find(const K& x) noexcept{ return this->find_impl(x); }

		[[nodiscard]] constexpr const_iterator find(const key_type& key) const noexcept{ return this->find_impl(key); }

		template <typename K>
		[[nodiscard]] constexpr const_iterator find(const K& x) const noexcept{
			return this->find_impl(x);
		}

		// Bucket interface
		[[nodiscard]] constexpr size_type bucket_count() const noexcept{ return buckets_.size(); }

		[[nodiscard]] constexpr size_type max_bucket_count() const noexcept{ return buckets_.max_size(); }

		// Hash policy
		constexpr void rehash(size_type count){
			count = std::max(count, size() * 2);
			open_hash_map other(*this, count);
			swap(*this, other);
		}

		constexpr void reserve(const size_type count){
			if(count * 2 > buckets_.size()){
				rehash(count * 2);
			}
		}

		// Observers
		[[nodiscard]] constexpr hasher hash_function() const noexcept{ return Hasher; }

		[[nodiscard]] constexpr key_equal key_eq() const noexcept{ return KeyEqualer; }

	private:
		template <std::convertible_to<key_type> K, typename... Args>
		constexpr std::pair<iterator, bool> emplace_impl(const K& key, Args&&... args){
			assert(!KeyEqualer(empty_key_, key) && "empty key shouldn't be used");

			reserve(size_ + 1);
			for(std::size_t idx = this->key_to_idx(key);; idx = probe_next(idx)){
				if(KeyEqualer(buckets_[idx].first, empty_key_)){
					buckets_[idx].second = mapped_type(std::forward<Args>(args)...);
					buckets_[idx].first = key;
					size_++;
					return {iterator(this, idx), true};
				} else if(KeyEqualer(buckets_[idx].first, key)){
					return {iterator(this, idx), false};
				}
			}
		}

		constexpr void erase_impl(const iterator it) noexcept{
			std::size_t bucket = it.idx_;
			for(std::size_t idx = probe_next(bucket);; idx = probe_next(idx)){
				if(KeyEqualer(buckets_[idx].first, empty_key_)){
					buckets_[bucket].first = empty_key_;
					size_--;
					return;
				}
				const std::size_t ideal = this->key_to_idx(buckets_[idx].first);
				if(diff(bucket, ideal) < diff(idx, ideal)){
					// swap, bucket is closer to ideal than idx
					buckets_[bucket] = buckets_[idx];
					bucket = idx;
				}
			}
		}

		template <typename K>
		constexpr size_type erase_impl(const K& key) noexcept{
			auto it = this->find_impl(key);
			if(it != end()){
				this->erase_impl(it);
				return 1;
			}
			return 0;
		}

		template <typename K>
		constexpr mapped_type& at_impl(const K& key){
			const auto it = this->find_impl(key);
			if(it != end()){
				return it->second;
			}
			throw std::out_of_range("HashMap::at");
		}

		template <typename K>
		constexpr const mapped_type& at_impl(const K& key) const{
			const auto it = this->find_impl(key);
			if(it != end()){
				return it->second;
			}
			throw std::out_of_range("HashMap::at");
		}

		template <typename K>
		constexpr std::size_t count_impl(const K& key) const noexcept{
			return this->find_impl(key) == end() ? 0 : 1;
		}

		template <typename K>
		constexpr iterator find_impl(const K& key) noexcept{
			static_assert(isEqualerValid<K>, "invalid equaler");
			assert(!KeyEqualer(empty_key_, key) && "empty key shouldn't be used");

			for(std::size_t idx = this->key_to_idx(key);; idx = probe_next(idx)){
				if(KeyEqualer(buckets_[idx].first, key)){
					return iterator(this, idx);
				}

				if(KeyEqualer(buckets_[idx].first, empty_key_)){
					return end();
				}
			}
		}

		template <typename K>
		constexpr const_iterator find_impl(const K& key) const noexcept{
			assert(!KeyEqualer(empty_key_, key) && "empty key shouldn't be used");

			for(std::size_t idx = this->key_to_idx(key);; idx = this->probe_next(idx)){
				if(KeyEqualer(buckets_[idx].first, key)){
					return const_iterator(this, idx);
				}

				if(KeyEqualer(buckets_[idx].first, empty_key_)){
					return end();
				}
			}
		}

		template <typename K>
		constexpr std::size_t key_to_idx(const K& key) const noexcept(noexcept(Hasher(key))){
			static_assert(isHasherValid<K>, "invalid hasher");
			const std::size_t mask = buckets_.size() - 1;
			return Hasher(key) & mask;
		}

		[[nodiscard]] constexpr std::size_t probe_next(const std::size_t idx) const noexcept{
			const std::size_t mask = buckets_.size() - 1;
			return (idx + 1) & mask;
		}

		[[nodiscard]] constexpr std::size_t diff(const std::size_t a, const std::size_t b) const noexcept{
			const std::size_t mask = buckets_.size() - 1;
			return (buckets_.size() + (a - b)) & mask;
		}

	private:
		/*static constexpr */
		key_type empty_key_{};
		buckets buckets_{};
		std::size_t size_{};
	};
}
