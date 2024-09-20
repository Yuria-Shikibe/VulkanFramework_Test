export module Graphic.Effect.Manager;

export import Graphic.Effect;

import ext.object_pool;
import ext.Container.ObjectPool;
import ext.heterogeneous;
import Geom.Rect_Orthogonal;

import std;

namespace Graphic{
	inline std::atomic_size_t next = 0;

	std::size_t allocateHandle(){
		return ++next;
	}

	/**
	 * @brief Basically, if no exception happens, once a effect is created, it won't be deleted!
	 * @details
	 * <p><b>
	 * Using Sequence: Synchronized
	 * </b><p><b>
	 *		update -> world acquire(parallel) -> rendering
	 * </b>
	 */
	export class EffectManager {
		using pool_type = ext::object_pool<Effect, 256>;
		pool_type effectPool{};

		std::vector<pool_type::unique_ptr> actives{};
		std::stack<pool_type::unique_ptr, std::vector<pool_type::unique_ptr>> pendings{};

		std::mutex acquire_ActivesMutex{};
		std::mutex acquire_PendingsMutex{};

	public:

		/**
		 * @param delta_in_ticks advanced time
		 */
		void update(const float delta_in_ticks){
			auto end = actives.end();
			for(auto itr = actives.begin(); itr < end; ++ itr){
				if(!itr->get()->update(delta_in_ticks))continue;

				pendings.push(std::move(*itr));

				end = std::prev(end);
				*itr = std::move(*end);
				actives.pop_back();
			}
		}

		/**
		 * @brief acquire an activated effect handle
		 * @return pointer to an activated effect, Guarantees NOT NULL
		 */
		[[nodiscard]] Effect* acquire(){
			Effect* out;

			{
				pool_type::unique_ptr ptr{};

				if(std::lock_guard lk{acquire_PendingsMutex}; !pendings.empty()){
					ptr = std::move(pendings.top());
					pendings.pop();
					goto active;
				}

				ptr = effectPool.obtain_unique();

				active:

				{
					std::lock_guard guard{acquire_ActivesMutex};
					out = ptr.get();
					actives.push_back(std::move(ptr));
				}
			}

			out->additionalData.reset();
			out->data.progress.time = 0;
			out->resignHandle(allocateHandle());

			return out;
		}

		void render(const Geom::OrthoRectFloat viewport) const{
			for(const auto& effect : actives){
				if(effect->drawer->getClipBound(*effect).overlap_Exclusive(viewport)){
					effect->render();
				}
			}
		}

		[[nodiscard]] bool isIdle() const noexcept{
			return actives.empty();
		}
	};
}
