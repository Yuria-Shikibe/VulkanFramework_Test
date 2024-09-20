module;

#include <cassert>

export module ext.views;

import std;

namespace ext::ranges{
	template <typename T, bool need_operator_bool = true>
		requires (std::is_object_v<T> && std::destructible<T>)
	class non_propagating_cache{
		// a simplified optional that resets on copy / move
	public:
		[[nodiscard]] constexpr non_propagating_cache() = default;

		constexpr non_propagating_cache(const non_propagating_cache&) noexcept{}

		non_propagating_cache(non_propagating_cache&& other) noexcept = default;

		non_propagating_cache& operator=(const non_propagating_cache& other) = default;

		non_propagating_cache& operator=(non_propagating_cache&& other) noexcept = default;

		[[nodiscard]] constexpr explicit operator bool() const noexcept
			requires need_operator_bool{
			return val.has_value();
		}

		[[nodiscard]] constexpr T& operator*() noexcept{
			return val.value();
		}

		[[nodiscard]] constexpr const T& operator*() const noexcept{
			return val.value();
		}

		template <class... Args>
		constexpr T& emplace(Args&&... _Args) noexcept(std::is_nothrow_constructible_v<T, Args...>){
			return val.emplace(std::forward<Args>(_Args)...);
		}

	private:
		std::optional<std::remove_cv_t<T>> val{};
	};

	template <typename T>
		requires (std::is_trivially_destructible_v<T> && std::is_object_v<T> && std::destructible<T>)
	class non_propagating_cache<T, false>{
	public:
		constexpr non_propagating_cache() noexcept{}

		constexpr non_propagating_cache(const non_propagating_cache&) noexcept{}

		constexpr non_propagating_cache(non_propagating_cache&&) noexcept{}

		constexpr non_propagating_cache& operator=(const non_propagating_cache&) noexcept{
			return *this;
		}

		constexpr non_propagating_cache& operator=(non_propagating_cache&&) noexcept{
			return *this;
		}

		[[nodiscard]] constexpr T& operator*() noexcept{
			return val;
		}

		[[nodiscard]] constexpr const T& operator*() const noexcept{
			return val;
		}

		template <class... Args>
		constexpr T& emplace(Args&&... _Args) noexcept(std::is_nothrow_constructible_v<T, Args...>){
			if(std::is_constant_evaluated()){
				std::construct_at(std::addressof(val), std::forward<Args>(_Args)...);
			} else{
				::new(static_cast<void*>(std::addressof(val))) T(std::forward<Args>(_Args)...);
			}

			return val;
		}

	private:
		std::remove_cv_t<T> val{};
	};

	export
	template <
		std::ranges::forward_range V,
		typename Pred,
		std::indirectly_unary_invocable<std::ranges::iterator_t<V>> Proj = std::identity
	>
		requires (std::indirect_unary_predicate<Pred, std::projected<std::ranges::iterator_t<V>, Proj>>)
	class split_if_view : public std::ranges::view_interface<split_if_view<V, Pred, Proj>>{
	private:
		/* [[no_unique_address]] */
		V range{};
		/* [[no_unique_address]] */
		Pred pred;
		/* [[no_unique_address]] */
		Proj proj;

		bool skipSplitter{};

		non_propagating_cache<std::ranges::iterator_t<V>> next{};

		class iterator{
		private:
			friend split_if_view;

			split_if_view* parent = nullptr;
			std::ranges::iterator_t<V> cur = {};
			std::ranges::iterator_t<V> next = {};

		public:
			using iterator_concept = std::forward_iterator_tag;
			using iterator_category = std::input_iterator_tag;
			using value_type = std::ranges::subrange<std::ranges::iterator_t<V>>;
			using difference_type = std::ranges::range_difference_t<V>;

			iterator() = default;

			constexpr iterator(split_if_view& parent, std::ranges::iterator_t<V> current,
			                   std::ranges::iterator_t<V> next)   //
				noexcept(std::is_nothrow_move_constructible_v<std::ranges::iterator_t<V>>) // strengthened
				: parent{std::addressof(parent)}, cur{std::move(current)}, next{std::move(next)}{}

			[[nodiscard]] constexpr std::ranges::iterator_t<V> base() const
				noexcept(std::is_nothrow_copy_constructible_v<std::ranges::iterator_t<V>>) /* strengthened */{
				return cur;
			}

			[[nodiscard]] constexpr value_type operator*() const
				noexcept(std::is_nothrow_copy_constructible_v<std::ranges::iterator_t<V>>) /* strengthened */{
				return {cur, next};
			}

			constexpr iterator& operator++(){
				const auto last = std::ranges::end(parent->range);
				cur = next;
				if(cur == last){
					return *this;
				}else if(parent->skipSplitter){
					++cur;
				}

				auto itrNext = std::ranges::find_if(cur, last, parent->pred, parent->proj);

				next = itrNext;
				return *this;
			}

			constexpr iterator operator++(int){
				auto tmp = *this;
				++*this;
				return tmp;
			}

			[[nodiscard]] constexpr friend bool operator==(const iterator& lhs, const iterator& rhs)
			noexcept(noexcept(lhs.cur == rhs.cur)){
				return lhs.cur == rhs.cur;
			}
		};

		class sentinel{
		private:
			/* [[no_unique_address]] */
			std::ranges::sentinel_t<V> last{};

			[[nodiscard]] constexpr bool eq(const iterator& itr) const
				noexcept(noexcept(itr.cur == last)){
				return itr.cur == last;
			}

		public:
			sentinel() = default;

			constexpr explicit sentinel(split_if_view& _Parent) noexcept(
				noexcept(std::ranges::end(_Parent.range))
				&& std::is_nothrow_move_constructible_v<std::ranges::sentinel_t<V>>) // strengthened
				: last(std::ranges::end(_Parent.range)){}

			[[nodiscard]] constexpr friend bool operator==(const iterator& itr, const sentinel& se) noexcept(
				noexcept(se.eq(itr))) /* strengthened */{
				return se.eq(itr);
			}
		};

		constexpr std::ranges::iterator_t<V> find_next(std::ranges::iterator_t<V> itr){
			const auto last = std::ranges::end(range);
			auto rst = std::ranges::find_if(itr, last, pred, proj);

			return rst;
		}

	public:
        // clang-format off
        split_if_view() requires
			std::default_initializable<V> && std::default_initializable<Pred> && std::default_initializable<Proj> = default;
		// clang-format on

		constexpr explicit split_if_view(
			V rngVal,
			Pred pred,
			Proj proj = {},
			const bool skipSplitter = false
		) noexcept(
			std::is_nothrow_move_constructible_v<V> && std::is_nothrow_move_constructible_v<Pred> &&
			std::is_nothrow_move_constructible_v<Proj>) // strengthened
			: range(std::move(rngVal)), pred(std::move(pred)), proj(std::move(proj)), skipSplitter{skipSplitter}{}

		[[nodiscard]] constexpr V base() const & noexcept(std::is_nothrow_copy_constructible_v<V>) /* strengthened */
			requires std::copy_constructible<V>{
			return range;
		}

		[[nodiscard]] constexpr V base() && noexcept(std::is_nothrow_move_constructible_v<V>) /* strengthened */{
			return std::move(range);
		}

		[[nodiscard]] constexpr auto begin(){
			auto first = std::ranges::begin(range);
			if(!next){
				next.emplace(split_if_view::find_next(first));
			}
			// ReSharper disable once CppDFANullDereference
			return iterator{*this, first, *next};
		}

		[[nodiscard]] constexpr auto end(){
			if constexpr(std::ranges::common_range<V>){
				return iterator{*this, std::ranges::end(range), {}};
			} else{
				return sentinel{*this};
			}
		}
	};

	template <
		std::ranges::forward_range Rng,
		typename Pred,
		std::indirectly_unary_invocable<std::ranges::iterator_t<Rng>> Proj = std::identity
	>
		requires (std::ranges::view<Rng> && std::indirect_unary_predicate<Pred, std::projected<std::ranges::iterator_t<Rng>, Proj>>)
	split_if_view(Rng&&, Pred&&, Proj&&) -> split_if_view<std::ranges::views::all_t<Rng>, std::decay_t<Pred>, std::decay_t<Proj>>;


	template <typename Func, typename... T>
	struct range_capture : std::ranges::range_adaptor_closure<range_capture<Func, T...>>{
		template <class... UTy>
			requires (std::same_as<std::decay_t<UTy>, T> && ...)
		constexpr explicit range_capture(UTy&&... args) noexcept(
			std::conjunction_v<std::is_nothrow_constructible<T, UTy>...>)
			: captures(std::forward<UTy>(args)...){}

		void operator()(auto&&) & = delete;

		void operator()(auto&&) const & = delete;

		void operator()(auto&&) && = delete;

		void operator()(auto&&) const && = delete;

		using Indices = std::index_sequence_for<T...>;

		template <class Ty>
			requires std::invocable<Func, Ty, T&...>
		constexpr decltype(auto) operator()(Ty&& arg) & noexcept(
			noexcept(range_capture::call(*this, std::forward<Ty>(arg), Indices{}))){
			return range_capture::call(*this, std::forward<Ty>(arg), Indices{});
		}

		template <class Ty>
			requires std::invocable<Func, Ty, const T&...>
		constexpr decltype(auto) operator()(Ty&& arg) const & noexcept(
			noexcept(range_capture::call(*this, std::forward<Ty>(arg), Indices{}))){
			return range_capture::call(*this, std::forward<Ty>(arg), Indices{});
		}

		template <class Ty>
			requires std::invocable<Func, Ty, T...>
		constexpr decltype(auto) operator()(Ty&& arg) && noexcept(
			noexcept(range_capture::call(std::move(*this), std::forward<Ty>(arg), Indices{}))){
			return range_capture::call(std::move(*this), std::forward<Ty>(arg), Indices{});
		}

		template <class Ty>
			requires std::invocable<Func, Ty, const T...>
		constexpr decltype(auto) operator()(Ty&& arg) const && noexcept(
			noexcept(range_capture::call(std::move(*this), std::forward<Ty>(arg), Indices{}))){
			return range_capture::call(std::move(*this), std::forward<Ty>(arg), Indices{});
		}

	private:
		template <class SelfTy, class Ty, size_t... Idx>
		static constexpr decltype(auto) call(SelfTy&& self, Ty&& arg, std::index_sequence<Idx...>) noexcept(
			noexcept(Func{}(std::forward<Ty>(arg), std::get<Idx>(std::forward<SelfTy>(self).captures)...))){
			return Func{}(std::forward<Ty>(arg), std::get<Idx>(std::forward<SelfTy>(self).captures)...);
		}

		std::tuple<T...> captures{};
	};

	namespace views{
		struct split_if_fn{
			template <
				std::ranges::viewable_range Rng,
				std::indirectly_unary_invocable<std::ranges::iterator_t<Rng>> Proj = std::identity,
				std::indirect_unary_predicate<std::projected<std::ranges::iterator_t<Rng>, Proj>> Pred
			>
			[[nodiscard]] constexpr auto operator()(
				Rng&& rng, Pred&& pred, Proj&& proj = {}, const bool skipSplitter = false) const
				noexcept(noexcept(split_if_view(std::forward<Rng>(rng), std::forward<Pred>(pred), std::forward<Proj>(proj))))
				requires requires{split_if_view(static_cast<Rng&&>(rng), static_cast<Pred&&>(pred), static_cast<Proj&&>(proj)); }{
					return split_if_view(std::forward<Rng>(rng), std::forward<Pred>(pred), std::forward<Proj>(proj), skipSplitter);
				}

			template <
				typename Proj = std::identity,
				typename Pred
			>
				requires requires{
				requires std::constructible_from<std::decay_t<Proj>, Proj>;
				requires std::constructible_from<std::decay_t<Pred>, Pred>;
				}
			[[nodiscard]] constexpr auto operator()(Pred&& pred, Proj&& proj = {}, const bool skipSplitter = false) const
				noexcept(
					std::is_nothrow_constructible_v<std::decay_t<Pred>, Pred> &&
					std::is_nothrow_constructible_v<std::decay_t<Proj>, Proj>){
				return range_capture<
						split_if_fn,
						std::decay_t<Pred>,
						std::decay_t<Proj>,
						bool>{
						std::forward<Pred>(pred), std::forward<Proj>(proj), skipSplitter
					};
			}
		};

		export constexpr split_if_fn split_if;
	}
}

namespace ext{
	export namespace views = ::ext::ranges::views;
}
