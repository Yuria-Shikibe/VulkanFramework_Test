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
T atomic_fetch_min_explicit(std::atomic<T>* pv, typename std::atomic<T>::value_type v, std::memory_order m) noexcept{
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
	constexpr float ContinuousTestScl = 1.25f;

	struct CollisionIndex{
		Index subject;
		Index object;
	};

	export
	struct CollisionData{
		//TODO provide posterior check?
		bool expired{};
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

	export
	class Hitbox{
		/** @brief CCD-traces size.*/
		Index size_CCD{};
		/** @brief CCD-traces clamp size. shrink only!*/
		mutable std::atomic<Index> sizeClamped_CCD{};

		/** @brief CCD-traces spacing*/
		Geom::Vec2 unitDisplacement_CCD{};

		mutable std::deque<CollisionData> collided{};

		//wrap box
		Geom::RectBoxBrief wrapBound_CCD{};
		Geom::OrthoRectFloat selfBound{};

	public:
		std::vector<HitBoxComponent> components{};
		Geom::Transform trans{};

		Hitbox() = default;

		[[nodiscard]] Hitbox(const std::vector<HitBoxComponent>& comps, const Geom::Transform transform = {})
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

		void updateHitbox(const Geom::Transform translation){
			this->trans = translation;

			const auto displayment = unitDisplacement_CCD * static_cast<float>(size_CCD);
			const float ang = displayment.angle();

			const float cos = Math::cosDeg(-ang);
			const float sin = Math::sinDeg(-ang);

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
					Geom::Vec2{minX, minY}.rotate(cos, -sin),
					Geom::Vec2{maxX, minY}.rotate(cos, -sin),
					Geom::Vec2{maxX, maxY}.rotate(cos, -sin),
					Geom::Vec2{minX, maxY}.rotate(cos, -sin)
				};

			selfBound = wrapBound_CCD.getMaxOrthoBound().shrinkBy(-displayment);
		}

		void updateHitboxWithCCD(const Geom::Transform translation){
			const auto move = translation.vec - this->trans.vec;

			updateHitbox(translation);
			genContinuousRectBox(move);
		}

		void move(const Geom::Vec2 move){
			updateHitbox({move + trans.vec, trans.rot});
			genContinuousRectBox(move);
		}

	private:
		[[nodiscard]] float fastGetSize2() const noexcept{
			return selfBound.maxDiagonalSqLen() * 0.35f;
		}

		/**
		 * \brief This ignores the rotation of the subject entity!
		 * Record backwards movements
		 */
		void genContinuousRectBox(
			Geom::Vec2 move
		){
			const float size2 = fastGetSize2();
			if(size2 < 0.025f) return;

			const float dst2 = move.length2();

			const int seg = Math::ceil(std::sqrtf(dst2 / size2) * ContinuousTestScl + std::numeric_limits<float>::epsilon());

			move /= static_cast<float>(seg);
			size_CCD = seg + 1;
			sizeClamped_CCD = size_CCD;
			unitDisplacement_CCD = -move;
			assert(!unitDisplacement_CCD.isNaN());
		}

		[[nodiscard]] bool isEnableCCD() const noexcept{
			return sizeClamped_CCD > 0;
		}

		[[nodiscard]] bool isEmptyCCD() const noexcept{
			return sizeClamped_CCD == 0;
		}

		void clampCCD(const Index index) const noexcept{
			atomic_fetch_min_explicit(&sizeClamped_CCD, index, std::memory_order_seq_cst);
		}

		[[nodiscard]] Index getClampedSizeCCD() const noexcept{
			// always >= 1
			return Math::max(sizeClamped_CCD.load(), 1u);
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

		void checkAllCollided() const noexcept{
			for(auto& checked_collision_data : collided){
				if(checked_collision_data.transition > getClampedSizeCCD()){
					checked_collision_data.expired = true;
				}
			}
		}

		void clearCollided() const noexcept{
			collided.clear();
		}

		/**
		 * @code
		 * | Index ++ <=> Distance ++ <=> Backwards ++
		 * + ----0----  ...  ----maxSize--->
		 * |    End              Initial
		 * |   State              State
		 * @endcode
		 * @param transIndex [0, @link getClampedSizeCCD() @endlink)
		 * @return Translation
		 */
		[[nodiscard]] constexpr Geom::Vec2 getDisplacement_CCD(const Index transIndex) const noexcept{
			assert(transIndex < size_CCD);
			return unitDisplacement_CCD * (static_cast<float>(size_CCD - transIndex) - 1.0f);
		}

		// [[nodiscard]] Geom::Vec2 getAvgEdgeNormal(const CollisionData data) const{
		// 	return Geom::avgEdgeNormal(data.intersection, hitBoxGroup.at(data.objectSubBoxIndex).box);
		// }

		[[nodiscard]] bool collideWithRough(const Hitbox& other) const noexcept{
			return wrapBound_CCD.overlapRough(other.wrapBound_CCD) && wrapBound_CCD.overlapExact(other.wrapBound_CCD);
		}

		[[nodiscard]] const CollisionData* collideWithExact(
			const Hitbox& other,
			const bool acquiresRecord = true,
			const bool soundful = false
		) const noexcept{
			constexpr static CollisionData Capture{[]{
				CollisionData data{};

				data.indices.push(CollisionIndex{});

				return data;
			}()};

			// const auto subjectCCD_size = sizeCCD_clamped();
			// const auto objectCCD_size = other.sizeCCD_clamped();

			Geom::OrthoRectFloat bound_subject = this->selfBound;
			Geom::OrthoRectFloat bound_object = other.selfBound;

			std::vector<Geom::RectBoxBrief> tempHitboxes{};

			{
				tempHitboxes.reserve(components.size() + other.components.size());
				tempHitboxes.append_range(components | std::views::transform(&HitBoxComponent::box));
				tempHitboxes.append_range(other.components | std::views::transform(&HitBoxComponent::box));
			}

			const auto rangeSubject = std::span{tempHitboxes.begin(), size()};
			const auto rangeObject = std::span{tempHitboxes.begin() + size(), tempHitboxes.end()};

			//initialize collision state
			{
				const auto maxTrans_subject = this->getDisplacement_CCD(0);
				const auto maxTrans_object = other.getDisplacement_CCD(0);

				//Move to initial stage
				for(auto& box : rangeSubject){
					box.move(maxTrans_subject);
				}

				for(auto& box : rangeObject){
					box.move(maxTrans_object);
				}

				bound_subject.move(maxTrans_subject);
				bound_object.move(maxTrans_object);
			}

			const auto trans_subject = -this->unitDisplacement_CCD;
			const auto trans_object = -other.unitDisplacement_CCD;

			Index lastCheckedIndex_subject{};
			Index lastCheckedIndex_object{};

			for(Index lastIndex_object{}, index_subject{}; index_subject < this->getClampedSizeCCD(); ++index_subject){
				//Calculate Move Step
				const float curRatio = static_cast<float>(index_subject + 1) / static_cast<float>(this->getClampedSizeCCD());
				const Index objectCurrentIndex = //The subject's size may shrink, cause the ratio larger than 1, resulting in array index out of bound
					Math::ceil(curRatio * static_cast<float>(other.getClampedSizeCCD()));

				assert(objectCurrentIndex <= other.size_CCD);

				for(Index index_object = lastIndex_object; index_object < objectCurrentIndex; ++index_object){
					//Perform CCD approach

					//Rough collision test 1;
					if(!bound_subject.overlap_Inclusive(bound_object)){
						bound_subject.move(trans_subject);
						bound_object.move(trans_object);

						continue;
					}

					bound_subject.move(trans_subject);
					bound_object.move(trans_object);

					//Rough test passed, move boxes to last valid position
					for(auto& box : rangeSubject){
						box.move(trans_subject * (index_subject - lastCheckedIndex_subject));
					}

					for(auto& box : rangeObject){
						box.move(trans_object * (index_object - lastCheckedIndex_object));
					}

					//Collision Test
					//TODO O(n^2) Is a small local quad tree necessary?

					CollisionData* collisionData{};

					for(auto&& [subjectBoxIndex, subject] : rangeSubject | std::views::enumerate){
						for(auto&& [objectBoxIndex, object] : rangeObject | std::views::enumerate){
							if(index_subject > this->getClampedSizeCCD() || index_object > other.getClampedSizeCCD()) return nullptr;

							if(!subject.overlapRough(object) || !subject.overlapExact(object)) continue;

							collisionData = &collided.emplace_back(index_subject);

							if(collisionData->empty()){
								this->clampCCD(index_subject);
								other.clampCCD(index_object);
							}

							if(acquiresRecord){
								collisionData->indices.push({static_cast<Index>(subjectBoxIndex), static_cast<Index>(objectBoxIndex)});
								if(!soundful || collisionData->indices.full()){
									return collisionData;
								}
							}else{
								return &Capture;
							}
						}
					}



					lastCheckedIndex_subject = index_subject;
					lastCheckedIndex_object = index_object;
				}

				lastIndex_object = objectCurrentIndex;
			}

			return nullptr;
		}

		//TODO shrink CCD bound
		//TODO shrink requires atomic...
		void fetchToLastClampPosition() noexcept{
			this->trans.vec.addScaled(unitDisplacement_CCD, static_cast<float>(sizeClamped_CCD.load()));
		}

		[[nodiscard]] bool contains(const Geom::Vec2 vec2) const noexcept{
			return std::ranges::any_of(components, [vec2](const HitBoxComponent& data){
				return data.box.contains(vec2);
			});
		}

		[[nodiscard]] constexpr float getRotationalInertia(const float mass, const float scale = 1 / 12.0f,
		                                                   const float lengthRadiusRatio = 0.25f) const noexcept{
			return std::accumulate(components.begin(), components.end(),
			                       1.0f, [mass, scale, lengthRadiusRatio](const float val, const HitBoxComponent& pair){
				                       return val + pair.box.getRotationalInertia(mass, scale, lengthRadiusRatio) + mass * pair.trans.vec.length2();
			                       });
		}

	private:
		static void transHitboxGroup(const std::span<HitBoxComponent> boxes, const Geom::Vec2 trans) noexcept{
			for(auto&& box : boxes){
				box.box.move(trans);
			}
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
