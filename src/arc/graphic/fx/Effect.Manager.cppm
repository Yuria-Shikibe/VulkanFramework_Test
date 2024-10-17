export module Graphic.Effect.Manager;

export import Graphic.Effect;

import ext.object_pool;
import ext.algo;
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
		// std::stack<pool_type::unique_ptr, std::vector<pool_type::unique_ptr>> pendings{};

		std::vector<pool_type::unique_ptr> buffer{};

		std::mutex acquire_BufferMutex{};
		std::mutex acquire_PendingsMutex{};

	public:

		void update(const float delta_in_ticks) noexcept{
			auto end = actives.end();
			auto cur = actives.begin();

			ext::algo::erase_if_unstable(actives, [delta_in_ticks](const pool_type::unique_ptr& e){
				return e->update(delta_in_ticks);
			});

			// while(cur != end){
			// 	if(!cur->get()->update(delta_in_ticks))goto next;
			//
			// 	// pendings.push(std::ranges::iter_move(cur));
			// 	--end;
			//
			// 	if(cur == end)break;
			// 	*cur = std::ranges::iter_move(end);
			//
			// 	next:
			// 	++cur;
			// }
			//
			// actives.erase(end, actives.end());
		}

		/**
		 * @brief acquire a TO BE activated effect handle
		 * @return ref to an activated effect
		 */
		[[nodiscard]] Effect& acquire() noexcept{
			Effect* out;

			pool_type::unique_ptr ptr{effectPool.obtain_unique()};

			{
				std::lock_guard guard{acquire_BufferMutex};
				out = ptr.get();
				buffer.push_back(std::move(ptr));
			}

			out->resignHandle(allocateHandle());

			return *out;
		}

		void dumpBuffer() noexcept{
			std::vector<pool_type::unique_ptr> dump{};
			{
				std::lock_guard guard{acquire_BufferMutex};
				dump = std::move(buffer);
			}

			actives.reserve(dump.size() + actives.size());
			std::ranges::move(dump, std::back_inserter(actives));
		}

		void dumpBuffer_unchecked() noexcept{
			actives.reserve(buffer.size() + actives.size());
			std::ranges::move(buffer, std::back_inserter(actives));
			buffer.clear();
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
