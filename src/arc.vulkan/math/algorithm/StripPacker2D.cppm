export module Math.Algo.StripPacker2D;


#if DEBUG_CHECK
import ext.RuntimeException;
#endif

import std;
import ext.Concepts;
import Geom.Rect_Orthogonal;
import Geom.Vector2D;
import Math;

export namespace Math {
    template <Concepts::Number N>
    struct Packable {
        using PackValueType = N;

        Geom::Rect_Orthogonal<N>& getBound() = delete;
    };

    struct T : Packable<std::uint32_t>{
        Geom::Rect_Orthogonal<PackValueType>& getBound() {

        }
    };

	/**
     * @brief
     * @tparam T Should Avoid Copy When Tgt Type is provided
     */
    template <Concepts::SpecDeriveOf<Packable> T>
		requires requires(T& t){
    		requires std::derived_from<T, Packable<typename T::PackValueType>>;
            { t.getBound() } -> std::same_as<Geom::Rect_Orthogonal<typename T::PackValueType>&>;
	    }
	struct StripPacker2D {
    protected:
        using N = typename T::PackValueType;
		using Rect = Geom::Rect_Orthogonal<N>;
    	using SubRectArr = std::array<Rect, 3>;
        using SizeTy = Geom::Vector2D<N>;
        using Ref = std::add_pointer_t<T>;

    	static Rect& obtain(Ref cont) noexcept {
			return cont->getBound();
		}

    	[[nodiscard]] bool contains(Ref cont) const noexcept {
    	    return all.contains(cont);
    	}

    	std::vector<Ref> boxes_widthAscend{};
    	std::vector<Ref> boxes_heightAscend{};
    	std::unordered_set<Ref> all{};
        std::vector<Ref> packed{};


    public:
        SizeTy exportSize{};
    	SizeTy maxSize{};

    	N margin{0};

    	constexpr void setMaxSize(const SizeTy size) noexcept {
    		maxSize.set(size);
    	}

    	constexpr void setMaxSize(const N maxWidth, const N maxHeight) noexcept {
    		maxSize.set(maxWidth, maxHeight);
    	}

		[[nodiscard]] StripPacker2D() = default;

        template <std::ranges::sized_range Rng>
            requires (std::same_as<Ref, std::ranges::range_value_t<Rng>>)
		[[nodiscard]] explicit StripPacker2D(Rng&& targets){
    		this->push<Rng>(std::forward<Rng>(targets));
    	}

    	template <std::ranges::sized_range Rng>
    		requires (std::same_as<Ref, std::ranges::range_value_t<Rng>>)
    	void push(Rng&& targets){
    	    auto size = std::ranges::size(targets);
    		all.reserve(size);
            packed.reserve(size);

            boxes_widthAscend = {std::ranges::begin(targets), std::ranges::end(targets)};
            boxes_heightAscend = {std::ranges::begin(targets), std::ranges::end(targets)};

    	    std::ranges::transform(targets, std::inserter(all, all.end()));
    	}

		void sortData() {
    		std::ranges::sort(boxes_widthAscend , std::greater<N>{}, [](const Ref& t){
    			return StripPacker2D::obtain(t).getWidth();
    		});

    		std::ranges::sort(boxes_heightAscend, std::greater<N>{}, [](const Ref& t){
				return StripPacker2D::obtain(t).getHeight();
			});
    	}

		[[nodiscard]] constexpr N getMargin() const noexcept{ return margin; }

		constexpr void setMargin(const N margin) noexcept{ this->margin = margin; }

		void process() noexcept {
    		sortData();

    		if(margin != 0){
    			for (auto& data : boxes_widthAscend){
    				this->obtain(data).addSize(margin * 2, margin * 2);
    			}
    		}
    		this->tryPlace({SubRectArr{Rect{}, Rect{}, Rect{maxSize}}});
    		if(margin != 0){
    			for (auto& data : boxes_widthAscend){
    				this->obtain(data).shrink(margin, margin);
    			}
    		}
    	}

    	Geom::Vector2D<N> getResultSize() const noexcept {return exportSize;}

    	auto& getRemains() noexcept {return all;}

    	auto& getPacked() noexcept {return packed;}

    protected:
    	[[nodiscard]] bool shouldStop() const noexcept {return all.empty();}

    	Rect* tryPlace(const Rect& bound, const std::vector<Ref>& targets) noexcept {
    		for(auto& cont : targets){
    			if(!this->contains(cont))continue;

    			Rect& rect = this->obtain(cont);

    			rect.setSrc(bound.getSrcX(), bound.getSrcY());

    			if( //Contains
					rect.getEndX() <= bound.getEndX() &&
					rect.getEndY() <= bound.getEndY()
				){
    				all.erase(cont);
    				packed.push_back(cont);

    				exportSize.maxX(rect.getEndX());
    				exportSize.maxY(rect.getEndY());

    				return &rect;
				}

    		}

    		return nullptr;
    	}


	    /**
    	 * @code
    	 *              |                                      
    	 *    array[0]  |  array[2]                            
    	 *              |                                
    	 * -------------|---------------
    	 *              |                      
    	 *      Box     |  array[1]                            
    	 *              |
    	 *
    	 * @endcode 
    	 */
        static constexpr SubRectArr splitQuad(const Rect& bound, const Rect& box) {
#if DEBUG_CHECK
    		if(bound.getSrcX() != box.getSrcX() || bound.getSrcY() != box.getSrcY())throw ext::IllegalArguments{"The source of the box and the bound doesn't match"};
#endif
    		return SubRectArr{
    			Rect{bound.getSrcX(),   box.getEndY(),   box.getWidth()                 , bound.getHeight() - box.getHeight()},
    			Rect{  box.getEndX(), bound.getSrcY(), bound.getWidth() - box.getWidth(),   box.getHeight()                  },
    			Rect{  box.getEndX(),   box.getEndY(), bound.getWidth() - box.getWidth(), bound.getHeight() - box.getHeight()}
    		};
    	}

    	void tryPlace(std::vector<SubRectArr>&& bounds) noexcept {
			if(shouldStop())return;

    		std::vector<SubRectArr> next{};
    		next.reserve(bounds.size() * 3);

    		//BFS
			for(const SubRectArr& currentBound : bounds) {
				for(const auto& [i, bound] : currentBound | std::ranges::views::enumerate){
					if(bound.area() == 0)continue;

					if(const Rect* result = this->tryPlace(bound, (i & 1) ? boxes_widthAscend : boxes_heightAscend)){
						//if(result != nullptr)
						auto arr = this->splitQuad(bound, *result);
						if(arr[0].area() == 0 && arr[1].area() == 0 && arr[2].area() == 0)continue;
						next.push_back(std::move(arr));
					}
				}
			}

    		if(!next.empty())this->tryPlace(std::move(next));
    	}
    };

}
