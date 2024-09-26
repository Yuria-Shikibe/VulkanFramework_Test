//
// Created by Matrix on 2024/9/25.
//

export module Core.UI.StatedLength;

export import Geom.Vector2D;

import std;

export namespace Core::UI{
	enum struct LengthDependency : std::uint8_t{
		master,
		masterRatio,
		slaving,
		slavingRatio,
		external,
		none,
	};

	struct StatedLength{
		float val{};
		LengthDependency dep{LengthDependency::slaving};

		[[nodiscard]] constexpr decltype(auto) getDependency() const noexcept{
			return std::to_underlying(dep);
		}

		constexpr void setConcreteSize(float val){
			this->val = val;
			dep = LengthDependency::master;
		}

		constexpr void promoteIndependence(const LengthDependency dependency) noexcept{
			if(dependency < dep){
				dep = dependency;
			}
		}

		[[nodiscard]] constexpr bool external() const noexcept{
			return dep == LengthDependency::external;
		}

		[[nodiscard]] constexpr bool dependent() const noexcept{
			return dep != LengthDependency::master;
		}

		[[nodiscard]] constexpr bool mastering() const noexcept{
			return dep == LengthDependency::master;
		}

		[[nodiscard]] constexpr bool isFromRatio() const noexcept{
			return dep == LengthDependency::slavingRatio || dep == LengthDependency::masterRatio;
		}

		[[nodiscard]] constexpr float crop() const noexcept{
			if(!mastering())return 0;
			return val;
		}
	};



	struct StatedSize{
		StatedLength x{}, y{};

		constexpr void checkValidity() const noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
			if(x.dep == y.dep){
				if(x.dep == LengthDependency::masterRatio || x.dep == LengthDependency::slavingRatio){
					throw std::invalid_argument("invalid size: circular ratio dependency.");
				}
			}
#endif
		}

		constexpr void trySetSlavedSize(const Geom::Vec2 size) noexcept{
			if(x.dep == LengthDependency::slaving){
				x.val = size.x;
				x.dep = LengthDependency::master;
			}
			if(y.dep == LengthDependency::slaving){
				y.val = size.y;
				y.dep = LengthDependency::master;
			}
		}


		constexpr void applyRatio(const Geom::Vec2 maxSize) noexcept{
			checkValidity();

			if(x.isFromRatio()){
				x.val = std::clamp(x.val * y.val, 0.f, maxSize.x);
				x.dep = y.dep;
			}

			if(y.isFromRatio()){
				y.val = std::clamp(x.val * y.val, 0.f, maxSize.y);
				y.dep = x.dep;
			}
		}

		constexpr void setConcreteSize(const Geom::Vec2 size) noexcept{
			x = {size.x, LengthDependency::master};
			y = {size.y, LengthDependency::master};
		}

		constexpr void setConcreteWidth(const float w) noexcept{
			x = {w, LengthDependency::master};
		}

		constexpr void setConcreteHeight(const float h) noexcept{
			y = {h, LengthDependency::master};
		}

		constexpr void setSize(const Geom::Vec2 size) noexcept{
			x.val = size.x;
			y.val = size.y;
		}

		[[nodiscard]] constexpr Geom::Vec2 getRawSize() const noexcept{
			return {x.val, y.val};
		}

		[[nodiscard]] constexpr Geom::Vec2 getConcreteSize() const noexcept{
			return {x.crop(), y.crop()};
		}

		constexpr void setState(const LengthDependency xState, const LengthDependency yState) noexcept{
			x.dep = xState;
			y.dep = yState;

			checkValidity();
		}

		constexpr std::pair<LengthDependency, LengthDependency> getState(){
			return {x.dep, y.dep};
		}

		[[nodiscard]] constexpr StatedSize cropMasterRatio() const noexcept{
			checkValidity();

			StatedSize crop{};
			crop.setState(x.dep, y.dep);

			switch(x.dep){
				case LengthDependency::masterRatio:{
					if(y.mastering()){
						crop.y.val = y.val;
						crop.x.val = y.val * x.val;
						crop.x.dep = crop.y.dep = LengthDependency::master;
					}else{
						crop.x.dep = LengthDependency::slavingRatio;
					}
					break;
				}
				case LengthDependency::master: crop.x = x; break;
				default: break;
			}

			switch(y.dep){
				case LengthDependency::masterRatio:{
					if(x.mastering()){
						crop.x.val = x.val;
						crop.y.val = x.val * y.val;
						crop.x.dep = crop.y.dep = LengthDependency::master;
					}else{
						crop.y.dep = LengthDependency::slavingRatio;
					}
					break;
				}
				case LengthDependency::master: crop.y = y; break;
				default: break;
			}

			return crop;
		}
	};

}
