//
// Created by Matrix on 2024/10/17.
//

export module Game.World.State;

export import Game.World.EntityGroup;
export import Game.World.Drawable;
export import Geom.quad_tree;

import std;
import ext.meta_programming;

namespace Game{
	export class RealEntity;

	export
	template <typename Fn>
	concept EntityInitFunc = requires{
		requires ext::is_complete<ext::function_traits<std::decay_t<Fn>>>;
		typename ext::function_arg_at_last<std::decay_t<Fn>>::type;
		requires std::is_lvalue_reference_v<typename ext::function_arg_at_last<std::decay_t<Fn>>::type>;
		requires std::derived_from<std::remove_cvref_t<typename ext::function_arg_at_last<std::decay_t<Fn>>::type>, Entity>;
	};

	export
	template <typename Fn>
	concept InvocableEntityInitFunc = requires{
		requires EntityInitFunc<Fn>;
		requires std::invocable<Fn, std::add_lvalue_reference_t<std::remove_cvref_t<typename ext::function_arg_at_last<std::decay_t<Fn>>::type>>>;
	};

	template <typename InitFunc>
	struct EntityInitFuncTraits{
		using EntityType = std::remove_cvref_t<typename ext::function_arg_at_last<std::decay_t<InitFunc>>::type>;
		static_assert(std::derived_from<EntityType, Entity>);
	};

	export
	struct WorldState{
		EntityGroup<Entity> all{};
		EntityGroup<RealEntity> realEntities{};

		Geom::quad_tree<RealEntity> quadTree{};


		void draw(Geom::OrthoRectFloat viewport) const;

		template <InvocableEntityInitFunc Fn>
		auto& add(Fn fn){
			using EntityType = typename EntityInitFuncTraits<Fn>::EntityType;
			std::shared_ptr<EntityType> entity{createEntityPtr<EntityType>()};

			std::invoke(fn, *entity);

			return this->pendEntityPtr<EntityType>(std::move(entity));
		}

		template <std::derived_from<Entity> Ty>
		Ty& add(){
			std::shared_ptr<Ty> entity{createEntityPtr<Ty>()};
			return this->pendEntityPtr<Ty>(std::move(entity));
		}

		void updateQuadTree();

	private:
		template <std::derived_from<Entity> Ty>
		std::shared_ptr<Ty> createEntityPtr(){
			std::shared_ptr<Ty> entity{};

			if constexpr (false){

			}else{
				entity = std::make_shared<Ty>();
			}

			return entity;
		}

		template <std::derived_from<Entity> Ty>
		Ty& pendEntityPtr(std::shared_ptr<Ty>&& entity){
			Ty& e = entity.operator*();

			if constexpr (std::derived_from<Ty, RealEntity>){
				realEntities.pend(entity);
			}

			all.pend(std::move(entity));

			return e;
		}

		template <typename T>
			requires std::derived_from<T, Drawable>
		static void drawGroup(const EntityGroup<T>& entities, const Geom::OrthoRectFloat viewport){
			for(const std::shared_ptr<T>& entity : entities.entities | std::views::values){
				if(viewport.overlap_Exclusive(entity->getClipRegion())){
					entity->draw();
				}
			}
		}
	};
};
