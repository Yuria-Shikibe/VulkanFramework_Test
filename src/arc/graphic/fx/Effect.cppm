export module Graphic.Effect;

export import Math.Timed;

import Geom.Transform;
import Geom.Rect_Orthogonal;
import Graphic.Color;
import ext.concepts;
import std;

namespace Core{
	export class Camera2D;
}

export namespace Graphic{
	struct Effect;
	class EffectManager;

	extern EffectManager& getDefManager();

	struct EffectDrawer{
		float defLifetime{60.0f};
		float defClipSize{100.0};

		constexpr virtual ~EffectDrawer() = default;

		constexpr EffectDrawer() = default;

		[[nodiscard]] constexpr explicit EffectDrawer(const float defaultLifetime)
			: defLifetime(defaultLifetime){}

		[[nodiscard]] constexpr EffectDrawer(const float defLifetime, const float defClipSize)
			: defLifetime{defLifetime},
			  defClipSize{defClipSize}{}

		[[nodiscard]] Effect& suspendOn(Graphic::EffectManager& manager) const;

		[[nodiscard]] Effect& suspendOn() const;

		virtual void operator()(Effect& effect) const = 0;

		[[nodiscard]] virtual Geom::OrthoRectFloat getClipBound(const Effect& effect) const noexcept;
	};

	/*template<Concepts::Invokable<void(Effect&)> Draw>
	struct EffectDrawer_Func final : EffectDrawer{
		const Draw func{};

		explicit EffectDrawer_Func(const float time, Draw&& func)
			: EffectDrawer(time), func(std::forward<Draw>(func)){}

		void operator()(Effect& effect) const override{
			func(effect);
		}
	};

	template <auto DrawFunc>
		requires requires{
			requires Concepts::Invokable<void(Effect&), decltype(DrawFunc)>;
		}
	struct EffectDrawer_StaticFunc final : EffectDrawer{
		explicit EffectDrawer_StaticFunc(const float time)
			: EffectDrawer(time){}

		void operator()(Effect& effect) const override{
			DrawFunc(effect);
		}
	};

	template <Concepts::Invokable<void(Effect&)> Func>
	EffectDrawer_Func(float, Func) -> EffectDrawer_Func<Func>;

	template<Concepts::Invokable<void(Effect&)> Draw, Concepts::Invokable<void(float, const Effect&)> Clip>
	struct EffectDrawer_Func_Clip final : EffectDrawer{
		const Draw func{};
		const Clip clip{};

		explicit EffectDrawer_Func_Clip(const float time, const float clipSize, Draw&& func, Clip&& clip)
			: EffectDrawer(clipSize, time), func(std::forward<Draw>(func)), clip(std::forward<Clip>(clip)){}

		void operator()(Effect& effect) const override{
			func(effect);
		}

		[[nodiscard]] Geom::OrthoRectFloat getClipBound(const Effect& effect) const noexcept override{
			return clip(defClipSize, effect);
		}
	};

	template <Concepts::Invokable<void(Effect&)> Func, Concepts::Invokable<void(float, const Effect&)> Clip>
	EffectDrawer_Func_Clip(float, float, Func, Clip) -> EffectDrawer_Func_Clip<Func, Clip>;


	struct EffectShake final : EffectDrawer{
		float near{1000};
		float far{10000};

		[[nodiscard]] constexpr EffectShake(const float defLifetime, float defClipSize)
			: EffectDrawer{defLifetime, defClipSize}{
			defClipSize = far * 2.f;
		}

		[[nodiscard]] constexpr EffectShake(){
			defClipSize = far * 2.f;
		}

		Effect* create(EffectManager* manager, Core::Camera2D* camera, const Geom::Vec2 pos, const float intensity, float fadeSpeed = -1.f) const;

		Effect* create(const Geom::Vec2 pos, const float intensity, const float fadeSpeed = -1.f) const;

		[[nodiscard]] constexpr float getIntensity(const float dst) const noexcept{
			return Math::clamp(1 - (1 / dst - 1 / near) / (1 / far - 1 / near));
		}

		void operator()(Effect& effect) const override;
	};

	template <Concepts::Invokable<void(Effect&)> Func>
	std::unique_ptr<EffectDrawer_Func<Func>> makeEffect(const float time, Func&& func){
		return std::make_unique<EffectDrawer_Func<Func>>(time, std::forward<Func>(func));
	}

	template<Concepts::Invokable<void(Effect&)> Draw, Concepts::Invokable<void(float, const Effect&)> Clip>
	std::unique_ptr<EffectDrawer_Func_Clip<Draw, Clip>> makeEffect(const float time, const float clipSize, Draw&& func, Clip&& clip){
		return std::make_unique<EffectDrawer_Func_Clip<Draw, Clip>>(time, clipSize, std::forward<Draw>(func), std::forward<Clip>(clip));
	}

	struct EffectDrawer_Multi final : EffectDrawer{
		std::vector<const EffectDrawer*> subEffects{};

		explicit EffectDrawer_Multi(const std::initializer_list<const EffectDrawer*> effects)
			: subEffects(effects){}

		void operator()(Effect& effect) const override{
			for (const auto subEffect : this->subEffects){
				subEffect->operator()(effect);
			}
		}
	};*/

	struct EffectBasicData{
		Math::Timed progress{};
		float zLayer{/*TODO default layer*/};

		Geom::Transform trans{};
		Color color{};
	};

	struct Effect{
		using HandleType = size_t;
		static constexpr float DefLifetime = -1.0f;

		EffectBasicData data{};
		HandleType handle{};
		std::any additionalData{};
		const EffectDrawer* drawer{nullptr};


		Effect& setData(const EffectBasicData& data){
			this->data = data;
			setLifetime(data.progress.lifetime);

			return *this;
		}

		Effect& setAdditional(std::any&& additonalData){
			this->additionalData = std::move(additonalData);

			return *this;
		}

		Effect& setLifetime(const float lifetime = DefLifetime){
			if(lifetime > 0.0f){
				data.progress.lifetime = lifetime;
			}else{
				if(drawer)data.progress.lifetime = drawer->defLifetime;
			}

			return *this;
		}

		Effect& setColor(const Color& color = Colors::WHITE){
			data.color = color;

			return *this;
		}

		Effect& setDrawer(const EffectDrawer* drawer){
			this->drawer = drawer;
			data.progress.set(0.0f, drawer->defLifetime);

			return *this;
		}

		void resignHandle(const std::size_t handle){
			this->handle = handle;
		}

		bool overrideRotation(HandleType& handle, const float rotation){
			if(!handle)return false;

			if(handle == this->handle){
				data.trans.rot = rotation;
				return true;
			}else{
				handle = 0;
				return false;
			}
		}

		bool overridePosition(HandleType& handle, const Geom::Vec2 position){
			if(!handle)return false;

			if(handle == this->handle){
				data.trans.vec = position;
				return true;
			}else{
				handle = 0;
				return false;
			}
		}

		[[nodiscard]] bool removable() const noexcept{
			return data.progress.time >= data.progress.lifetime;
		}

		[[nodiscard]] float getX() const noexcept{
			return data.trans.vec.x;
		}

		[[nodiscard]] float getY() const noexcept{
			return data.trans.vec.y;
		}

		[[nodiscard]] Geom::Vec2 pos() const noexcept{
			return data.trans.vec;
		}

		/**
		 * @return Whether this effect is removable
		 */
		[[nodiscard]] bool update(const float delta){
			return data.progress.advance(delta);
		}

		void render(){
			drawer->operator()(*this);
		}
	};
}
