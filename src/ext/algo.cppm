module;

// #include <type_traits>

export module ext.algo;
import ext.concepts;
import std;


namespace ext::algo{
	export
	template <
		std::input_iterator It,
		std::sentinel_for<It> Se,
		std::indirectly_unary_invocable<It> Proj = std::identity,
		typename Func = std::plus<>,
		typename T = std::indirect_result_t<Proj, It>>
		requires requires(Func func, T v, It it, Proj proj){
			{ std::invoke(func, std::move(v), std::invoke(proj, *it)) } -> std::convertible_to<T>;
		}
	constexpr T accumulate(It it, Se se, T initial = {}, Func func = {}, Proj proj = {}){
		for(; it != se; ++it){
			initial = std::invoke(func, std::move(initial), std::invoke(proj, *it));
		}

		return initial;
	}

	export
	template <
		typename Rng,
		typename Proj = std::identity, typename Func = std::plus<>,
		typename T = std::indirect_result_t<Proj, std::ranges::iterator_t<Rng>>>
	requires (!std::sentinel_for<T, Rng>)
	constexpr T accumulate(Rng&& rng, T initial, Func func = {}, Proj proj = {}){
		return ext::algo::accumulate(std::ranges::begin(rng), std::ranges::end(rng), std::move(initial), std::move(func), std::move(proj));
	}

	export
	template <typename Range>
		requires ext::range_of<Range>
	auto partBy(Range&& range, ext::Invokable<bool(const std::ranges::range_value_t<Range>&)> auto&& pred){
		return std::make_pair(range | std::ranges::views::filter(pred),
			range | std::ranges::views::filter(std::not_fn(pred)));
	}

	template <typename Itr, typename Sentinel>
	constexpr bool itrSentinelCompariable = requires(Itr itr, Sentinel sentinel){
		{ itr < sentinel } -> std::convertible_to<bool>;
	};

	template <typename Itr, typename Sentinel>
	constexpr bool itrSentinelCheckable = requires(Itr itr, Sentinel sentinel){
		{ itr != sentinel } -> std::convertible_to<bool>;
	};

	template <typename Itr, typename Sentinel>
	constexpr bool itrRangeOrderCheckable = requires(Itr itr, Sentinel sentinel){
		{ itr <= sentinel } -> std::convertible_to<bool>;
	};

	template <typename Itr, typename Sentinel>
	constexpr void itrRangeOrderCheck(const Itr& itr, const Sentinel& sentinel){
		if constexpr(itrRangeOrderCheckable<Itr, Sentinel>){
			if(!(itr <= sentinel)){
				throw std::runtime_error{"iterator transposed"};
			}
		}
	}

	template <bool replace, typename Src, typename Dst>
	void swapItr(Src& src, Dst& dst){
		if constexpr(replace){
			*dst = std::move(*src);
		} else{
			std::iter_swap(src, dst);
		}
	}


	template <
		bool replace,
		std::permutable Itr,
		std::permutable Sentinel,
		typename Proj = std::identity,
		std::indirect_unary_predicate<std::projected<Itr, Proj>> Pred
	>
	requires requires(Sentinel sentinel){--sentinel; requires std::sentinel_for<Sentinel, Itr>;}
	[[nodiscard]] constexpr Itr
		remove_if_unstable_impl(Itr first, Sentinel sentinel, Pred&& pred, const Proj porj = {}){
		algo::itrRangeOrderCheck(first, sentinel);

		Itr firstFound = std::ranges::find_if(first, sentinel, pred, porj);
		Sentinel sl = sentinel;

		if(firstFound != sentinel){
			--sl;
			while(pred(std::invoke(porj, *sl))) --sl;

			if constexpr(itrSentinelCompariable<Itr, Sentinel>){
				while(firstFound < sl){
					algo::swapItr<replace>(sl, firstFound);

					firstFound = std::ranges::find_if(firstFound, sl, pred, porj);
					while(pred(std::invoke(porj, *sl))) --sl;
				}
			} else{
				while(firstFound != sentinel){
					if(pred(std::invoke(porj, *firstFound))){
						algo::swapItr<replace>(sentinel, firstFound);
						if(!(--sentinel != firstFound)) break;
					} else{
						++firstFound;
					}
				}
			}
		}

		return firstFound;
	}

	template <bool replace, std::permutable Itr, std::permutable Sentinel, typename Ty, typename Proj = std::identity>
		requires requires(Sentinel sentinel){
		--sentinel;
		requires std::sentinel_for<Sentinel, Itr>;
	}
	[[nodiscard]] constexpr Itr remove_unstable_impl(Itr first, Sentinel sentinel, const Ty& val, const Proj porj = {}){
		if constexpr(requires{
			algo::remove_if_unstable_impl<replace>(first, sentinel, std::bind_front(std::equal_to<Ty>{}, val), porj);
		}){
			return algo::remove_if_unstable_impl<replace>(first, sentinel, std::bind_front(std::equal_to<Ty>{}, val), porj);
		} else{
			return algo::remove_if_unstable_impl<replace>(first, sentinel, std::bind_front(std::equal_to<>{}, val), porj);
		}
	}

	template <bool replace, std::permutable Itr, std::permutable Sentinel, typename Proj = std::identity,
	          std::indirect_unary_predicate<std::projected<Itr, Proj>> Pred>
	requires requires(Sentinel sentinel){--sentinel; requires std::sentinel_for<Sentinel, Itr>;}
	[[nodiscard]] constexpr Itr remove_unique_if_unstable_impl(Itr first, Sentinel sentinel, Pred&& pred,
		const Proj porj = {}){
		algo::itrRangeOrderCheck(first, sentinel);

		Itr firstFound = std::ranges::find_if(first, sentinel, pred, porj);
		if(firstFound != sentinel){
			--sentinel;

			if(firstFound != sentinel){
				if(pred(std::invoke(porj, *firstFound))){
					algo::swapItr<replace>(sentinel, firstFound);
				}
				++firstFound;
			}
		}

		return sentinel;
	}

	template <bool replace, std::permutable Itr, std::permutable Sentinel, typename Ty, typename Proj = std::identity>
	requires requires(Sentinel sentinel){--sentinel; requires std::sentinel_for<Sentinel, Itr>;}
	[[nodiscard]] constexpr Itr remove_unique_unstable_impl(Itr first, Sentinel sentinel, const Ty& val,
		const Proj porj = {}){

		if constexpr(requires{
			algo::remove_unique_if_unstable_impl<replace>(first, sentinel, std::bind_front(std::equal_to<Ty>{}, val), porj);
		}){
			return algo::remove_unique_if_unstable_impl<replace>(first, sentinel, std::bind_front(std::equal_to<Ty>{}, val), porj);
		} else{
			return algo::remove_unique_if_unstable_impl<replace>(first, sentinel, std::bind_front(std::equal_to<>{}, val), porj);
		}
	}

	export
	template <bool replace = false, std::permutable Itr, std::permutable Sentinel, typename Ty, typename Proj =
	          std::identity>
	[[nodiscard]] constexpr auto remove_unstable(Itr first, const Sentinel sentinel, const Ty& val,
		const Proj porj = {}){
		return std::ranges::subrange<Itr>{
				algo::remove_unstable_impl<replace>(first, sentinel, val, porj), sentinel
			};
	}

	export
	template <bool replace = false, std::ranges::random_access_range Rng, typename Ty, typename Proj = std::identity>
	[[nodiscard]] constexpr auto remove_unstable(Rng& range, const Ty& val, const Proj porj = {}){
		return std::ranges::borrowed_subrange_t<Rng>{
				algo::remove_unstable_impl<replace>(std::ranges::begin(range), std::ranges::end(range), val, porj),
				std::ranges::end(range)
			};
	}

	export
	template <bool replace = false, std::permutable Itr, std::permutable Sentinel, typename Proj = std::identity,
	          std::indirect_unary_predicate<std::projected<Itr, Proj>> Pred>
	[[nodiscard]] constexpr auto remove_if_unstable(Itr first, const Sentinel sentinel, Pred&& pred,
		const Proj porj = {}){
		return std::ranges::subrange<Itr>{
				algo::remove_if_unstable_impl<replace>(first, sentinel, pred, porj), sentinel
			};
	}

	export
	template <bool replace = false, std::ranges::random_access_range Rng, typename Proj = std::identity,
	          std::indirect_unary_predicate<std::projected<std::ranges::iterator_t<Rng>, Proj>> Pred>
	[[nodiscard]] constexpr auto remove_if_unstable(Rng& range, Pred&& pred, const Proj porj = {}){
		return std::ranges::borrowed_subrange_t<Rng>{
				algo::remove_if_unstable_impl<replace>(std::ranges::begin(range), std::ranges::end(range), pred,
					porj),
				std::ranges::end(range)
			};
	}

	export
	template <bool replace = false, std::ranges::random_access_range Rng, typename Ty, typename Proj = std::identity>
		requires requires(Rng rng){
			requires std::ranges::sized_range<Rng>;
			rng.erase(std::ranges::begin(rng), std::ranges::end(rng)); requires std::ranges::sized_range<Rng>;
		}
	constexpr decltype(auto) erase_unstable(Rng& range, const Ty& val, const Proj porj = {}){
		auto oldSize = range.size();
		range.erase(
			algo::remove_unstable_impl<replace>(std::ranges::begin(range), std::ranges::end(range), val, porj),
			std::ranges::end(range));
		return oldSize - range.size();
	}

	export
	template <bool replace = false, std::ranges::random_access_range Rng, typename Proj = std::identity,
	          std::indirect_unary_predicate<std::projected<std::ranges::iterator_t<Rng>, Proj>> Pred>
		requires requires(Rng rng){
			requires std::ranges::sized_range<Rng>;
			rng.erase(std::ranges::begin(rng), std::ranges::end(rng)); requires std::ranges::sized_range<Rng>;
		}
	constexpr decltype(auto) erase_if_unstable(Rng& range, Pred&& pred, const Proj porj = {}){
		auto oldSize = range.size();
		range.erase(
			algo::remove_if_unstable_impl<replace>(std::ranges::begin(range), std::ranges::end(range), pred, porj),
			std::ranges::end(range));
		return oldSize - range.size();
	}

	export
	template <bool replace = false, std::ranges::random_access_range Rng, typename Ty, typename Proj = std::identity>
		requires requires(Rng rng){
			requires std::ranges::sized_range<Rng>;
			rng.erase(std::ranges::begin(rng), std::ranges::end(rng)); requires std::ranges::sized_range<Rng>;
		}
	constexpr decltype(auto) erase_unique_unstable(Rng& range, const Ty& val, const Proj porj = {}){
		auto oldSize = range.size();
		range.erase(
			algo::remove_unique_unstable_impl<replace>(std::ranges::begin(range), std::ranges::end(range), val,
				porj),
			std::ranges::end(range));
		return oldSize - range.size();
	}

	export
	template <bool replace = false, std::ranges::random_access_range Rng, typename Proj = std::identity,
	          std::indirect_unary_predicate<std::projected<std::ranges::iterator_t<Rng>, Proj>> Pred>
		requires requires(Rng rng){
			requires std::ranges::sized_range<Rng>;
			rng.erase(std::ranges::begin(rng), std::ranges::end(rng)); requires std::ranges::sized_range<Rng>;
		}
	constexpr decltype(auto) erase_unique_if_unstable(Rng& range, Pred&& pred, const Proj porj = {}){
		auto oldSize = range.size();
		range.erase(
			algo::remove_unique_if_unstable_impl<
				replace>(std::ranges::begin(range), std::ranges::end(range), pred, porj), std::ranges::end(range));
		return oldSize - range.size();
	}

}
