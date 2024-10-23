module;

#include "../src/ext/adapted_attributes.hpp"

export module Game.Entity.HitBox;

export import Geom.Shape.RectBox;
import Geom.Transform;
import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import Geom.Matrix3D;
import Math;
import Geom;

import std;
import ext.array_stack;

using Index = unsigned;

constexpr std::memory_order drop_release(const std::memory_order m) noexcept{
	return m == std::memory_order_release
		       ? std::memory_order_relaxed
		       : m == std::memory_order_acq_rel || m == std::memory_order_seq_cst
		       ? std::memory_order_acquire
		       : m;
}

template <typename T>
T atomic_fetch_min_explicit(std::atomic<T>* pv, typename std::atomic<T>::value_type v, const std::memory_order m) noexcept{
	auto const mr = drop_release(m);
	auto t = (mr != m) ? pv->fetch_add(0, m) : pv->load(mr);
	while(std::min(v, t) != t){
		if(pv->compare_exchange_weak(t, v, m, mr)) return t;
	}
	return t;
}

namespace Game{
	//TODO no mutable
	//TODO json srl
	/**
	 * \brief Used of CCD precision, the larger - the slower and more accurate
	 */
	constexpr float ContinuousTestScl = 1.33f;

	export
	struct CollisionIndex{
		Index subject;
		Index object;
	};

	export
	struct CollisionData{
		//TODO provide posterior check?
		//TODO set expired by set transition to an invalid index or clear indices instead of using a bool flag?
		Index transition{};
		ext::array_stack<CollisionIndex, 4> indices{};

		[[nodiscard]] CollisionData() = default;

		[[nodiscard]] CollisionData(const Index transition)
			: transition(transition){
		}

		[[nodiscard]] constexpr bool empty() const noexcept{
			return indices.empty();
		}
	};

	export
	struct HitBoxComponent{
		/** @brief Local Transform */
		Geom::Transform trans{};

		/** @brief Raw Box Data */
		Geom::RectBox box{};
	};

	struct HitBoxModel{
		std::vector<HitBoxComponent> hitBoxGroup{};
	};

	union HitboxSpan{
		std::span<Geom::RectBoxBrief> temporary;
		std::span<const HitBoxComponent> components{};
	};

	export
	template <typename T>
	concept CollisionCollector = requires(T t){
		requires std::is_invocable_r_v<CollisionData*, T, Index>;
	};

	struct CollisionNoCollect{
		constexpr CollisionData* operator()(Index) const noexcept{
			assert(false);
			return nullptr;
		}
	};


	export
	class Hitbox{
		/** @brief CCD-traces size.*/
		Index size_CCD{1};
		/** @brief CCD-traces clamp size. shrink only!*/
		mutable std::atomic<Index> indexClamped_CCD{};

		/** @brief CCD-traces spacing*/
		Geom::Vec2 backtraceUnitMove{};

		//wrap box
		Geom::RectBoxBrief wrapBound_CCD{};
		Geom::OrthoRectFloat selfBound{};

	public:
		std::vector<HitBoxComponent> components{};
		Geom::Transform trans{};

		void updateHitbox(const Geom::Transform translation){
			assert(!components.empty());

			this->trans = translation;

			const auto displayment = backtraceUnitMove * static_cast<float>(size_CCD);
			const float ang = displayment.equalsTo(Geom::ZERO) ? +0.f : displayment.angle();

			const float cos = Math::cosDeg(-ang);
			const float sin = Math::sinDeg(-ang);

			// const float cos = std::cos(ang * Math::DEGREES_TO_RADIANS);
			// const float sin = std::sin(ang * Math::DEGREES_TO_RADIANS);

			float minX = std::numeric_limits<float>::max();
			float minY = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float maxY = std::numeric_limits<float>::lowest();

			//TODO Simple object [box == 1] Optimization (for bullets mainly)

			for(auto& boxData : components){
				boxData.box.update(boxData.trans | trans);

				const std::array verts{
						boxData.box.v0.copy().rotate(cos, sin),
						boxData.box.v1.copy().rotate(cos, sin),
						boxData.box.v2.copy().rotate(cos, sin),
						boxData.box.v3.copy().rotate(cos, sin)
					};

				for(auto [x, y] : verts){
					minX = Math::min(minX, x);
					minY = Math::min(minY, y);
					maxX = Math::max(maxX, x);
					maxY = Math::max(maxY, y);
				}
			}

			CHECKED_ASSUME(minX <= maxX);
			CHECKED_ASSUME(minY <= maxY);

			maxX += displayment.length();

			wrapBound_CCD = Geom::RectBoxBrief{
					Geom::Vec2{minX, minY} .rotate(cos, -sin),
					Geom::Vec2{maxX, minY} .rotate(cos, -sin),
					Geom::Vec2{maxX, maxY} .rotate(cos, -sin),
					Geom::Vec2{minX, maxY} .rotate(cos, -sin)
				};

			selfBound = wrapBound_CCD.getMaxOrthoBound().shrinkBy(-displayment);
		}

		void updateHitboxWithCCD(const Geom::Transform translation){
			// const auto lastTrans = trans;
			const auto move = translation.vec - this->trans.vec;

			genContinuousRectBox(move);
			updateHitbox(translation);
		}

		void move(const Geom::Vec2 move){
			updateHitbox({move + trans.vec, trans.rot});
			genContinuousRectBox(move);
		}


		[[nodiscard]] float estimateMaxLength2() const noexcept{
			return selfBound.maxDiagonalSqLen() * 0.35f;
		}

	private:


		/**
		 * \brief This ignores the rotation of the subject entity!
		 * Record backwards movements
		 */
		void genContinuousRectBox(
			Geom::Vec2 move
		){
			const float size2 = estimateMaxLength2();
			if(size2 < 0.025f){
				return;
			}

			const float dst2 = move.length2();

			const Index seg = Math::min(Math::round<Index>(std::sqrtf(dst2 / size2) * ContinuousTestScl), 64u);

			if(seg > 0){
				move /= static_cast<float>(seg);
				backtraceUnitMove = -move;
			}else{
				backtraceUnitMove = {};
			}

			// std::println(std::cerr, "{}", seg);

			size_CCD = seg + 1;
			indexClamped_CCD.store(seg, std::memory_order::release);
		}

		void clampCCD(const Index index) const noexcept{
			atomic_fetch_min_explicit(&indexClamped_CCD, index, std::memory_order::acq_rel);
		}

	public:
		void scl(const Geom::Vec2 scl){
			for(auto& data : components){
				data.box.offset *= scl;
				data.box.size *= scl;
			}
		}

		[[nodiscard]] Geom::OrthoRectFloat getMaxWrapBound() const noexcept{
			return wrapBound_CCD.getMaxOrthoBound();
		}

		[[nodiscard]] Geom::OrthoRectFloat getMinWrapBound() const noexcept{
			return selfBound;
		}

		[[nodiscard]] auto& getWrapBox() const noexcept{
			return wrapBound_CCD;
		}

		[[nodiscard]] constexpr std::size_t size() const noexcept{
			return components.size();
		}

		[[nodiscard]] Index getBackTraceIndex() const noexcept{
			return indexClamped_CCD.load(std::memory_order::acquire);
		}

		/**
		 * @code
		 * | Index ++ <=> Distance ++ <=> Backwards ++
		 * + ----0----  ...  ----maxSize--->
		 * |    End              Initial
		 * |   State              State
		 * @endcode
		 * @param transIndex [0, @link getClampedIndexCCD() @endlink)
		 * @return Translation
		 */
		[[nodiscard]] constexpr Geom::Vec2 getBackTraceMoveAt(const Index transIndex) const noexcept{
			if(size_CCD == transIndex)return {};
			assert(transIndex < size_CCD);
			return backtraceUnitMove * (size_CCD - transIndex - 1);
		}

		[[nodiscard]] constexpr Geom::Vec2 getBackTraceUnitMove() const noexcept{
			return backtraceUnitMove;
		}


		[[nodiscard]] Geom::Vec2 getBackTraceMove() const noexcept{
			return getBackTraceMoveAt(getBackTraceIndex());
		}

		// [[nodiscard]] Geom::Vec2 getAvgEdgeNormal(const CollisionData data) const{
		// 	return Geom::avgEdgeNormal(data.intersection, hitBoxGroup.at(data.objectSubBoxIndex).box);
		// }

		[[nodiscard]] bool collideWithRough(const Hitbox& other) const noexcept{
			return wrapBound_CCD.overlapRough(other.wrapBound_CCD) && wrapBound_CCD.overlapExact(other.wrapBound_CCD);
		}

		//TODO remake this bullshit...
		template <CollisionCollector Col = CollisionNoCollect>
		bool collideWithExact(
			const Hitbox& other,
			Col collector = {},
			const bool soundful = false
		) const noexcept{
			Geom::OrthoRectFloat bound_subject = this->selfBound;
			Geom::OrthoRectFloat bound_object = other.selfBound;

			const auto trans_subject = -this->backtraceUnitMove;
			const auto trans_object = -other.backtraceUnitMove;

			const bool sbjHasTrans = backtraceUnitMove.isZero();
			const bool objHasTrans = other.backtraceUnitMove.isZero();


			std::vector<Geom::RectBoxBrief> tempHitboxes{};
			HitboxSpan rangeSubject{};
			HitboxSpan rangeObject{};

			if(sbjHasTrans && objHasTrans){
				rangeSubject.components = components;
				rangeObject.components = other.components;
			}else if(!sbjHasTrans && objHasTrans){
				tempHitboxes.reserve(components.size());
				tempHitboxes.append_range(components | std::views::transform(&HitBoxComponent::box));

				rangeSubject.temporary = tempHitboxes;
				rangeObject.components = other.components;
			}else if(sbjHasTrans && !objHasTrans){
				tempHitboxes.reserve(other.components.size());
				tempHitboxes.append_range(other.components | std::views::transform(&HitBoxComponent::box));

				rangeSubject.components = components;
				rangeObject.temporary = tempHitboxes;
			}else{
				tempHitboxes.reserve(components.size() + other.components.size());
				tempHitboxes.append_range(components | std::views::transform(&HitBoxComponent::box));
				tempHitboxes.append_range(other.components | std::views::transform(&HitBoxComponent::box));

				rangeSubject.temporary = std::span{tempHitboxes.begin(), size()};;
				rangeObject.temporary = std::span{tempHitboxes.begin() + size(), tempHitboxes.end()};;
			}


			//initialize collision state
			{
				if(!sbjHasTrans){
					const auto maxTrans_subject = this->getBackTraceMoveAt(0);

					//Move to initial stage
					for(auto& box : rangeSubject.temporary){
						box.move(maxTrans_subject);
					}

					bound_subject.move(maxTrans_subject);
				}

				if(!objHasTrans){
					const auto maxTrans_object = other.getBackTraceMoveAt(0);

					for(auto& box : rangeObject.temporary){
						box.move(maxTrans_object);
					}

					bound_object.move(maxTrans_object);
				}
			}

			assert(size_CCD > 0);
			assert(other.size_CCD > 0);

			Index index_object{};
			Index index_subject{};

			auto sbjMoveFunc = [&]{
				if(sbjHasTrans)return;
				for(auto& box : rangeSubject.temporary){
					box.move(trans_subject);
				}
				bound_subject.move(trans_subject);
			};

			auto objMoveFunc = [&]{
				if(objHasTrans)return;
				for(auto& box : rangeObject.temporary){
					box.move(trans_object);
				}
				bound_object.move(trans_object);
			};

			//OPTM this is horrible...

			for(; index_subject <= getBackTraceIndex(); ++index_subject){
				const auto curIdx = other.getBackTraceIndex();

				for(; index_object <= curIdx; ++index_object){
					if(!bound_subject.overlap_Exclusive(bound_object)){
						goto next0;
					}

					if(this->loadTo<Col>(
						collector, soundful, other,
						rangeSubject, index_subject,
						rangeObject, index_object
					)){
						return true;
					}

					next0:
					objMoveFunc();
				}

				if(!bound_subject.overlap_Exclusive(bound_object)){
					goto next1;
				}

				if(this->loadTo<Col>(
					collector, soundful, other,
					rangeSubject, index_subject,
					rangeObject, index_object
				)){
					return true;
				}

				next1:
				sbjMoveFunc();

				if(!objHasTrans){
					for(auto& box : rangeObject.temporary){
						box.move(-trans_object * (curIdx + 1));
					}

					bound_object.move(-trans_object * (curIdx + 1));
				}
			}

			auto curIdx = other.getBackTraceIndex();

			for(; index_object <= curIdx; ++index_object){
				if(!bound_subject.overlap_Exclusive(bound_object)){
					goto next2;
				}

				if(this->loadTo<Col>(
					collector, soundful, other,
					rangeSubject, index_subject,
					rangeObject, index_object
				)){
					return true;
				}

				next2:
				objMoveFunc();
			}

			if(!bound_subject.overlap_Exclusive(bound_object)){
				return false;
			}

			if(this->loadTo<Col>(
				collector, soundful, other,
				rangeSubject, index_subject,
				rangeObject, index_object
			)){
				return true;
			}

			return false;
			//
			// for(; index_subject <= this->getClampedIndexCCD() + 1; ++index_subject){
			// 	const auto ratio = static_cast<float>(index_subject/* + 1*/) / static_cast<float>(size_CCD);
			// 	const auto ceilIndex = static_cast<Index>(Math::ceil(ratio * other.size_CCD));
			//
			// 	assert(ratio >= 0.f && ratio <= 1.f);
			//
			// 	const auto dst = lastCeilIndex - lastBeginIdx2;
			// 	if(lastCeilIndex != ceilIndex){
			// 		if(dst == 1){
			// 			for(auto& box : rangeObject){
			// 				box.move(trans_object);
			// 			}
			// 			bound_object.move(trans_object);
			// 			++objMove;
			// 		}
			//
			// 		//ceil changed
			// 		lastBeginIdx2 = lastCeilIndex;
			// 		lastCeilIndex = ceilIndex;
			// 	}else{
			// 		if(dst > 1){
			// 			for(auto& box : rangeObject){
			// 				box.move(trans_object * -dst);
			// 			}
			// 			bound_object.move(trans_object * -dst);
			// 			objMove -= dst;
			// 		}
			// 	}
			//
			// 	for(index_object = lastBeginIdx2; index_object <= ceilIndex; ++index_object){
			// 		if(this->loadTo<Col>(
			// 			collector, soundful, other,
			// 			rangeSubject, index_subject,
			// 			rangeObject, index_object
			// 		)){
			// 			return true;
			// 		}
			//
			// 		next:
			// 		if(ceilIndex - lastBeginIdx2 > 1){
			// 			for(auto& box : rangeObject){
			// 				box.move(trans_object);
			// 			}
			// 			bound_object.move(trans_object);
			// 			++objMove;
			// 		}
			// 	}
			//
			// 	for(auto& box : rangeSubject){
			// 		box.move(trans_subject);
			// 	}
			// 	bound_subject.move(trans_subject);
			// 	++sbjMove;
			// }
			//
			// return false;
		}

	private:
		template <CollisionCollector Col = CollisionNoCollect>
		bool loadTo(
			Col collector, const bool soundful, const Hitbox& other,
			const HitboxSpan rangeSubject, const Index index_subject,
			const HitboxSpan rangeObject, const Index index_object
		) const noexcept{
			CollisionData* collisionData{};
			assert(index_subject <= size_CCD);
			assert(index_object <= other.size_CCD);

			// if(!bound_subject.overlap_Inclusive(bound_object)){
			// 	goto next;
			// }

			auto tryIntersect = [&](
				const std::span<Geom::RectBoxBrief>::size_type subjectBoxIndex, const Geom::RectBoxBrief& subject,
				const std::span<Geom::RectBoxBrief>::size_type objectBoxIndex, const Geom::RectBoxBrief& object
			){
				if(index_subject > this->getBackTraceIndex() + 1 || index_object > other.getBackTraceIndex() + 1) return false;

				if(!subject.overlapRough(object) || !subject.overlapExact(object)) return false;

				if constexpr (std::same_as<Col, CollisionNoCollect>){
					this->clampCCD(index_subject);
					other.clampCCD(index_object);

					return true;
				}

				if(!collisionData){
					this->clampCCD(index_subject);
					other.clampCCD(index_object);
					collisionData = std::invoke(collector, index_subject);
				}

				collisionData->indices.push({static_cast<Index>(subjectBoxIndex), static_cast<Index>(objectBoxIndex)});
				if(!soundful || collisionData->indices.full()){
					return true;
				}
			};


			if(backtraceUnitMove.isZero() && other.backtraceUnitMove.isZero()){
				for(auto&& [subjectBoxIndex, subject] : rangeSubject.components | std::views::transform(&HitBoxComponent::box) | std::views::enumerate){
					for(auto&& [objectBoxIndex, object] : rangeObject.components | std::views::transform(&HitBoxComponent::box) | std::views::enumerate){
						if(tryIntersect(subjectBoxIndex, subject, objectBoxIndex, object)){
							return true;
						}
					}
				}
			}else if(!backtraceUnitMove.isZero() && other.backtraceUnitMove.isZero()){
				for(auto&& [subjectBoxIndex, subject] : rangeSubject.temporary | std::views::enumerate){
					for(auto&& [objectBoxIndex, object] : rangeObject.components | std::views::transform(&HitBoxComponent::box) | std::views::enumerate){
						if(tryIntersect(subjectBoxIndex, subject, objectBoxIndex, object)){
							return true;
						}
					}
				}
			}else if(backtraceUnitMove.isZero() && !other.backtraceUnitMove.isZero()){
				for(auto&& [subjectBoxIndex, subject] : rangeSubject.components | std::views::transform(&HitBoxComponent::box) | std::views::enumerate){
					for(auto&& [objectBoxIndex, object] : rangeObject.temporary | std::views::enumerate){
						if(tryIntersect(subjectBoxIndex, subject, objectBoxIndex, object)){
							return true;
						}
					}
				}
			}else{
				for(auto&& [subjectBoxIndex, subject] : rangeSubject.temporary | std::views::enumerate){
					for(auto&& [objectBoxIndex, object] : rangeObject.temporary | std::views::enumerate){
						if(tryIntersect(subjectBoxIndex, subject, objectBoxIndex, object)){
							return true;
						}
					}
				}
			}

			assert(soundful || !collisionData);
			if(collisionData){
				assert(!collisionData->empty());
				return true;
			}

			return false;
		}

	public:

		[[nodiscard]] bool contains(const Geom::Vec2 vec2) const noexcept{
			return std::ranges::any_of(components, [vec2](const HitBoxComponent& data){
				return data.box.contains(vec2);
			});
		}

		[[nodiscard]] constexpr float getRotationalInertia(
			const float mass, const float scale = 1 / 12.0f,
			const float lengthRadiusRatio = 0.25f) const noexcept{
			return std::ranges::fold_left(components, 1.0f, [mass, scale, lengthRadiusRatio](const float val, const HitBoxComponent& pair){
				return val + pair.box.getRotationalInertia(mass, scale, lengthRadiusRatio) + mass * pair.trans.vec.length2();
			});
		}

	public:

		Hitbox() = default;

		[[nodiscard]] explicit Hitbox(const std::vector<HitBoxComponent>& comps, const Geom::Transform transform = {})
			: components(comps), trans(transform){
			updateHitbox(transform);
		}

		[[nodiscard]] explicit Hitbox(const HitBoxComponent& comps, const Geom::Transform transform = {})
			: Hitbox(std::vector{comps}, transform){
		}

		Hitbox(const Hitbox& other)
			: wrapBound_CCD{other.wrapBound_CCD},
			  selfBound{other.selfBound},
			  components{other.components},
			  trans{other.trans}{
		}

		Hitbox(Hitbox&& other) noexcept
			: wrapBound_CCD{std::move(other.wrapBound_CCD)},
			  selfBound{std::move(other.selfBound)},
			  components{std::move(other.components)},
			  trans{std::move(other.trans)}{
		}

		Hitbox& operator=(const Hitbox& other){
			if(this == &other) return *this;
			wrapBound_CCD = other.wrapBound_CCD;
			selfBound = other.selfBound;
			components = other.components;
			trans = other.trans;
			return *this;
		}

		Hitbox& operator=(Hitbox&& other) noexcept{
			if(this == &other) return *this;
			wrapBound_CCD = std::move(other.wrapBound_CCD);
			selfBound = std::move(other.selfBound);
			components = std::move(other.components);
			trans = std::move(other.trans);
			return *this;
		}
	};

	//
	// template <std::derived_from<HitBoxComponent> T>
	// void flipX(std::vector<T>& datas){
	// 	const std::size_t size = datas.size();
	//
	// 	std::ranges::copy(datas, std::back_inserter(datas));
	//
	// 	for(int i = 0; i < size; ++i){
	// 		auto& dst = datas.at(i + size);
	// 		dst.trans.vec.y *= -1;
	// 		dst.trans.rot *= -1;
	// 		dst.trans.rot = Math::Angle::getAngleInPi2(dst.trans.rot);
	// 		dst.box.offset.y *= -1;
	// 		dst.box.sizeVec2.y *= -1;
	// 	}
	// }
	//
	// void scl(std::vector<HitBoxComponent>& datas, const float scl){
	// 	for (auto& data : datas){
	// 		data.box.offset *= scl;
	// 		data.box.sizeVec2 *= scl;
	// 		data.trans.vec *= scl;
	// 	}
	// }
	//
	// void flipX(HitBox& datas){
	// 	flipX(datas.hitBoxGroup);
	// }
	//
	// void read(const OS::File& file, HitBox& box){
	// 	box.hitBoxGroup.clear();
	// 	int size = 0;
	//
	// 	std::ifstream reader{file.getPath(), std::ios::binary | std::ios::in};
	//
	// 	reader.read(reinterpret_cast<char*>(&size), sizeof(size));
	//
	// 	box.hitBoxGroup.resize(size);
	//
	// 	for (auto& data : box.hitBoxGroup){
	// 		data.read(reader);
	// 	}
	//
	// }
}
//
// export template<>
// struct ext::json::JsonSerializator<Geom::RectBox>{
// 	static void write(ext::json::JsonValue& jsonValue, const Geom::RectBox& data){
// 		ext::json::append(jsonValue, ext::json::keys::Size2D, data.sizeVec2);
// 		ext::json::append(jsonValue, ext::json::keys::Offset, data.offset);
// 		ext::json::append(jsonValue, ext::json::keys::Trans, data.transform);
// 	}
//
// 	static void read(const ext::json::JsonValue& jsonValue, Geom::RectBox& data){
// 		ext::json::read(jsonValue, ext::json::keys::Size2D, data.sizeVec2);
// 		ext::json::read(jsonValue, ext::json::keys::Offset, data.offset);
// 		ext::json::read(jsonValue, ext::json::keys::Trans, data.transform);
// 	}
// };
//
// export template<>
// struct ext::json::JsonSerializator<Game::HitBoxComponent>{
// 	static void write(ext::json::JsonValue& jsonValue, const Game::HitBoxComponent& data){
// 		ext::json::append(jsonValue, ext::json::keys::Trans, data.trans);
// 		ext::json::append(jsonValue, ext::json::keys::Data, data.box);
// 	}
//
// 	static void read(const ext::json::JsonValue& jsonValue, Game::HitBoxComponent& data){
// 		ext::json::read(jsonValue, ext::json::keys::Trans, data.trans);
// 		ext::json::read(jsonValue, ext::json::keys::Data, data.box);
// 	}
// };
//
// export template<>
// struct ext::json::JsonSerializator<Game::HitBox>{
// 	static void write(ext::json::JsonValue& jsonValue, const Game::HitBox& data){
// 		ext::json::JsonValue boxData{};
// 		ext::json::JsonSrlContBase_vector<decltype(data.hitBoxGroup)>::write(boxData, data.hitBoxGroup);
//
// 		ext::json::append(jsonValue, ext::json::keys::Data, std::move(boxData));
// 		ext::json::append(jsonValue, ext::json::keys::Trans, data.trans);
// 	}
//
// 	static void read(const ext::json::JsonValue& jsonValue, Game::HitBox& data){
// 		const auto& map = jsonValue.asObject();
// 		const ext::json::JsonValue boxData = map.at(ext::json::keys::Data);
// 		ext::json::JsonSrlContBase_vector<decltype(data.hitBoxGroup)>::read(boxData, data.hitBoxGroup);
// 		ext::json::read(jsonValue, ext::json::keys::Trans, data.trans);
// 		data.updateHitbox(data.trans);
// 	}
// };
