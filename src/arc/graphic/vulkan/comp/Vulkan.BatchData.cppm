module;

#include <vulkan/vulkan.h>

export module Graphic.BatchData;

import std;
import ext.concepts;

export namespace Graphic{
	using ImageIndex = std::uint8_t;

	struct DrawArgs{
		ImageIndex imageIndex{};
		std::uint32_t validCount{};
		void* dataPtr{};
	};

	struct LockableDrawArgs{
		ImageIndex imageIndex{};
		std::uint32_t validCount{};
		void* dataPtr{};
		std::shared_lock<std::shared_mutex> captureLock{};
	};


	/*template<typename T, std::size_t GroupCount = 4>
	struct DrawArgsView : std::ranges::view_interface<DrawArgsView<T, GroupCount>>, std::ranges::view_base{
		static constexpr std::size_t UnitOffset = GroupCount * sizeof(T);

		const LockableDrawArgs* args{};

		[[nodiscard]] DrawArgsView() = default;

		[[nodiscard]] explicit DrawArgsView(const LockableDrawArgs* args)
			: args{args}{}

		using value_type = std::array<T, GroupCount>;
		using iterator = value_type*;

		auto size() const noexcept{
			return args->validCount;
		}

		auto data() const noexcept{
			return static_cast<value_type*>(args->dataPtr);
		}

		auto begin() const noexcept{
			return data();
		}

		auto end() const noexcept{
			return data() + size();
		}
	};*/

	template <typename ArgTy>
	struct DrawArgsGroup{
		std::vector<ArgTy> args{};

		// template <typename T, std::size_t GroupCount = 4>
		// decltype(auto) as(){
		// 	return args | std::views::transform([](const auto& arg){return DrawArgsView<T, GroupCount>{&arg};}) | std::views::join;
		// }

		// template <typename T, std::size_t GroupCount = 4>
		// using view_iterator = std::ranges::iterator_t<decltype(std::declval<DrawArgsGroup&>().as<T, GroupCount>())>;

		template <std::ranges::sized_range Rng>
		void append(Rng&& drawArgs){
			args.reserve(args.size() + std::ranges::size(drawArgs));
			std::ranges::move(std::move(drawArgs), std::back_inserter(args));
		}

		template <typename T>
			requires (std::same_as<std::decay_t<T>, ArgTy>)
		void append(T&& drawArgs){
			args.push_back(std::forward<T>(drawArgs));
		}
	};


	template<typename ArgTy, typename T, typename Dv>
	struct AutoDrawSpaceAcquirer{
		static constexpr std::size_t GroupCount = 4;
		static constexpr std::size_t UnitOffset = GroupCount * sizeof(T);

		using value_type = std::array<T, GroupCount>;

		std::size_t capacity{};

		DrawArgsGroup<ArgTy> argsGroup{};

		void append(std::vector<ArgTy>&& drawArgs){
			for (const auto& count : drawArgs | std::views::transform(&ArgTy::validCount)){
				capacity +=	count;
			}

			argsGroup.append(std::move(drawArgs));
		}

		void append(ArgTy&& drawArgs) requires std::is_trivially_copy_assignable_v<ArgTy>{
			argsGroup.append(drawArgs);
		}

		//TODO auto unlock
		struct iterator{
			//OPTM uses iterator instead of index?
			//TODO impl +=, -=, +, -
			AutoDrawSpaceAcquirer* src{};
			std::uint32_t groupIndex{};
			std::uint32_t localIndex{};

			iterator& operator++(){
				++localIndex;

				auto& group = src->argsGroup.args[groupIndex];

				if(localIndex >= group.validCount){
					if constexpr (requires(ArgTy t){
						t.captureLock.unlock();
					}){
						group.captureLock.unlock();
					}

					localIndex = 0;
					groupIndex++;
				}

				return *this;
			}

			iterator operator++(int){
				iterator i{*this};

				this->operator++();

				return i;
			}

			iterator& operator--(){
				if(localIndex == 0){
					if(groupIndex){
						groupIndex--;
						localIndex = src->argsGroup.args[groupIndex].validCount - 1;
					}else{
						throw std::underflow_error("Invalid group index");
					}
				}else{
					--localIndex;
				}

				return *this;
			}

			iterator operator--(int){
				iterator i{*this};
				this->operator--();
				return i;
			}

			std::strong_ordering operator <=>(const iterator& o) const noexcept{
				if(groupIndex < o.groupIndex){
					return std::strong_ordering::less;
				}

				if(groupIndex > o.groupIndex){
					return std::strong_ordering::greater;
				}

				return localIndex <=> o.localIndex;
			}

			bool operator==(const iterator& o) const noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
				if(src != o.src)throw std::runtime_error("Source is not the same");
#endif
				return groupIndex == o.groupIndex && localIndex == o.localIndex;
			}

			std::pair<value_type*, ImageIndex> operator* () const{
				auto& arg = src->argsGroup.args.at(groupIndex);
				return {static_cast<value_type*>(arg.dataPtr) + localIndex, arg.imageIndex};
			}
		};

		iterator begin(){
			return {this, 0, 0};
		}

		iterator end(){
			return {this, static_cast<std::uint32_t>(argsGroup.args.size()), 0u};
		}

		Dv& crtpSelf(){
			return *static_cast<Dv*>(this);
		}

		auto size() const noexcept{
			return capacity;
		}

		void reserve(std::size_t count) = delete;
	};


	template <ext::spec_of<AutoDrawSpaceAcquirer> T>
	struct BasicAcquirer{
		using iterator = typename T::iterator;

		iterator current{};
		iterator last{};
		iterator end{};

		[[nodiscard]] explicit BasicAcquirer(T& src)
			: current{src.begin()},
			  end{src.end()}{}

		//TODO reserve
		decltype(auto) next(){
			if(current == end){
				reserve(1);
			}
			auto data = current.operator*();
			last = current;
			++current;
			return data;
		}

		void reserve(const std::size_t size){
			current.src->crtpSelf().reserve(size);
			end = current.src->end();
		}
	};
}
