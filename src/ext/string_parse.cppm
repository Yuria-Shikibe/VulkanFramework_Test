//
// Created by Matrix on 2024/9/9.
//

export module ext.algo.string_parse;

import std;

namespace ext::algo{


	export
	template <std::size_t count, std::ranges::range Rng = std::string_view, typename SplitterTy, std::ranges::viewable_range TargetRng>
	constexpr std::array<Rng, count> split_to(TargetRng&& rng, SplitterTy&& splitter){
		std::array<Rng, count> rst{};

		auto view = std::forward<TargetRng>(rng)
			| std::views::split(std::forward<SplitterTy>(splitter))
			| std::views::take(count)
			| std::views::transform([]<typename T>(T&& subRange){
				return Rng{std::ranges::begin(std::forward<T>(subRange)), std::ranges::end(std::forward<T>(subRange))};
			});

		std::ranges::move(view, rst.begin());

		return rst;
	}



	export struct unwrap_result{
		using layer_type = std::string_view;
		using element_type = std::string_view::value_type;

		using offset = layer_type::size_type;

		std::vector<unwrap_result> sub_layers{};

		layer_type layer{};
		offset begin_pos{};
		offset end_pos{};

		[[nodiscard]] unwrap_result() = default;

		[[nodiscard]] unwrap_result(const layer_type& layer, const offset begin_pos)
			: layer{layer},
			  begin_pos{begin_pos}, end_pos{begin_pos + layer.size()}{}


		[[nodiscard]] constexpr bool is_flat() const noexcept{
			return sub_layers.empty();
		}

		[[nodiscard]] constexpr offset size() const noexcept{
			return layer.size();
		}

		[[nodiscard]] constexpr bool is_leaf() const noexcept{
			return sub_layers.empty();
		}

		[[nodiscard]] constexpr std::string remove_subs() const{
			std::string rst{layer};
			std::string::size_type off{};

			for(const auto& sub_layer : sub_layers){
				rst.erase(sub_layer.begin_pos - off, sub_layer.size());
				off += sub_layer.size();
			}

			return rst;
		}

		[[nodiscard]] constexpr std::string remove_wrapped_subs() const{
			std::string rst{layer};
			std::string::size_type off{};

			for(const auto& sub_layer : sub_layers){
				rst.erase(sub_layer.begin_pos - off - 1, sub_layer.size() + 2);
				off += sub_layer.size() + 2;
			}

			return rst;
		}

		[[nodiscard]] constexpr std::vector<std::string> get_clean_layers() const{
			std::vector<std::string> strs{};
			collect_remove_wrapped_subs(strs);
			return strs;
		}

	private:
		constexpr void collect_remove_wrapped_subs(std::vector<std::string>& strs) const{
			for (const auto & sub_layer : sub_layers){
				sub_layer.collect_remove_wrapped_subs(strs);
			}

			strs.push_back(remove_wrapped_subs());
		}
	};

	export bool begin_with_digit(const std::string_view sv){
		return !sv.empty() && std::isdigit(sv.front());
	}


	export unwrap_result unwrap(const unwrap_result::layer_type target, const unwrap_result::element_type split){
		unwrap_result rst{target, 0ll};

		unwrap_result* last = &rst;

		unwrap_result::layer_type::difference_type l = 0;
		unwrap_result::layer_type::difference_type r = target.size() - 1;

		while(true){
			while(l < r && target[l] != split)++l;
			while(l < r && target[r] != split)--r;

			++l;
			if(l > r)return rst;
			auto v = target.substr(l, r - l);

			last = &last->sub_layers.emplace_back(v, l - last->begin_pos);
			--r;
		}
	}

	export unwrap_result unwrap(unwrap_result::layer_type target, const unwrap_result::element_type subBegin,  const unwrap_result::element_type subEnd){
		unwrap_result rst{target, 0ll};

		std::stack<unwrap_result*> layers{};
		layers.push(&rst);

		unwrap_result::layer_type::size_type currentOffset{};

		auto current = [&]{return layers.top();};

		for (auto [idx, value] : target | std::views::enumerate){
			if(value == subBegin){
				currentOffset = current()->begin_pos;
				auto* cur = &current()->sub_layers.emplace_back(target.substr(idx + 1), static_cast<unwrap_result::offset>(idx + 1));
				layers.push(cur);
			}

			if(value == subEnd){
				auto* top = current();
				top->begin_pos -= currentOffset;
				top->end_pos = idx - currentOffset;
				top->layer.remove_suffix(target.size() - idx);

				layers.pop();

				if(layers.empty()){
					throw std::invalid_argument("cannot unwrap string in wrong format");
				}

				currentOffset = current()->begin_pos;
			}
		}

		return rst;
	}
}
