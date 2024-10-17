module;

#include <cassert>
#include "../src/ext/adapted_attributes.hpp"

export module Geom.quad_tree;

export import Geom.Rect_Orthogonal;
export import Geom.Vector2D;

import ext.concepts;
import ext.RuntimeException;
import std;
export import Geom.QuadTree.Interface;

namespace Geom{
	//TODO flattened quad tree with pre allocated sub trees
	constexpr std::size_t MaximumItemCount = 4;

	template <std::size_t layers>
	struct quad_tree_mdspan_trait{
		using index_type = unsigned;

		template <std::size_t>
		static constexpr std::size_t sub_size = 4;

		static consteval auto extent_impl(){
			return [] <std::size_t... I>(std::index_sequence<I...>){
				return std::extents<index_type, sub_size<I>...>{};
			}(std::make_index_sequence<layers>());
		}

		static consteval std::size_t expected_size(){
			return [] <std::size_t... I>(std::index_sequence<I...>){
				return (sub_size<I> * ...);
			}(std::make_index_sequence<layers>());
		}

		using extent_type = decltype(extent_impl());
		static constexpr std::size_t required_span_size = expected_size();
	};


	template <typename ItemTy, ext::number T = float>
		requires ext::derived<ItemTy, QuadTreeAdaptable<ItemTy, T>>
	struct quad_tree_evaluateable_traits{
		using rect_type = Rect_Orthogonal<T>;

		static constexpr bool HasRoughIntersect = requires(const ItemTy& value){
			{ value.roughIntersectWith(value) } -> std::same_as<bool>;
		};

		static constexpr bool HasExactIntersect = requires(const ItemTy& value){
			{ value.exactIntersectWith(value) } -> std::same_as<bool>;
		};

		static constexpr bool HasPointIntersect = requires(const ItemTy& value, Geom::Vector2D<T> p){
			{ value.containsPoint(p) } -> std::same_as<bool>;
		};

		static rect_type bound_of(const ItemTy& cont) noexcept{
			static_assert(requires(ItemTy value){
				{ value.getBound() } -> std::same_as<rect_type>;
			}, "QuadTree Requires ValueType impl at `Rect getBound()` member function");

			return cont.getBound();
		}

		static bool isIntersectedBetween(const ItemTy& subject, const ItemTy& object) noexcept{
			//TODO equalTy support?
			if(&subject == &object) return false;

			bool intersected = quad_tree_evaluateable_traits::bound_of(subject).overlap_Exclusive(quad_tree_evaluateable_traits::bound_of(object));

			if constexpr(HasRoughIntersect){
				if(intersected) intersected &= subject.roughIntersectWith(object);
			}

			if constexpr(HasExactIntersect){
				if(intersected) intersected &= subject.exactIntersectWith(object);
			}

			return intersected;
		}

		static bool isIntesectedWithPoint(const Vec2 point, const ItemTy& object) noexcept requires (HasPointIntersect){
			return quad_tree_evaluateable_traits::bound_of(object).containsPos_edgeExclusive(point) && object.containsPoint(point);
		}
	};


	struct always_true{
		template <typename T>
		constexpr bool operator()(const T&, const T&) const noexcept{
			return true;
		}
	};

	template <typename ItemTy, ext::number T = float>
		requires std::derived_from<ItemTy, QuadTreeAdaptable<ItemTy, T>>
	struct basic_quad_tree_node{
		using rect_type = Rect_Orthogonal<T>;
		using trait = quad_tree_evaluateable_traits<ItemTy, T>;

		rect_type boundary{};
		std::vector<ItemTy*> items{};
		unsigned branch_size = 0;
		bool leaf = true;

		[[nodiscard]] basic_quad_tree_node() = default;

		[[nodiscard]] explicit basic_quad_tree_node(const rect_type& boundary)
			: boundary(boundary){
		}

		[[nodiscard]] rect_type get_boundary() const noexcept{ return boundary; }

		[[nodiscard]] unsigned size() const noexcept{ return branch_size; }

		[[nodiscard]] bool is_leaf() const noexcept{ return leaf; }

		[[nodiscard]] bool has_valid_children() const noexcept{
			return !leaf && branch_size != items.size();
		}

		[[nodiscard]] auto& get_items() const noexcept{ return items; }

		[[nodiscard]] bool contains(const rect_type object) const noexcept{
			return boundary.containsLoose(object);
		}

		[[nodiscard]] bool contains(const ItemTy& object) const noexcept{
			return boundary.containsLoose(trait::bound_of(object));
		}

		[[nodiscard]] bool overlaps(const ItemTy& object) const noexcept{
			return boundary.overlap_Exclusive(trait::bound_of(object));
		}

		[[nodiscard]] bool contains(const Vec2 object) const noexcept{
			return boundary.containsPos_edgeInclusive(object);
		}

	protected:
		[[nodiscard]] bool is_children_cached() const noexcept = delete;

		void split() = delete;

		void unsplit() = delete;
	};

	// constexpr std::size_t quad_tree_layers = 10;

	// template <typename ItemTy, ext::number T = float>
	// 	requires std::derived_from<ItemTy, QuadTreeAdaptable<ItemTy, T>>
	// struct static_quad_tree_node{
	// 	using extent_traits = quad_tree_mdspan_trait<quad_tree_layers>;
	// 	using rect_type = Rect_Orthogonal<T>;
	// 	using node_type = basic_quad_tree_node<ItemTy, T>;
	//
	// private:
	// 	rect_type boundary{};
	//
	// 	std::array<node_type, extent_traits::required_span_size> nodes{};
	// 	std::mdspan<node_type,  extent_traits::extent_type> span{nodes.data()};
	// public:
	// 	[[nodiscard]] static_quad_tree_node() = default;
	//
	// 	[[nodiscard]] explicit static_quad_tree_node(const rect_type boundary)
	// 		: boundary(boundary){
	// 		auto subRect = this->boundary.split();
	//
	// 	}
	// };
	//
	//
	// struct EmptyV : QuadTreeAdaptable<EmptyV, float>{
	// 	[[nodiscard]] Rect_Orthogonal<float> getBound() const noexcept{
	// 		return {};
	// 	}
	//
	// 	[[nodiscard]] bool roughIntersectWith(const EmptyV& other) const = delete;
	//
	// 	[[nodiscard]] bool exactIntersectWith(const EmptyV& other) const = delete;
	//
	// 	[[nodiscard]] bool containsPoint(typename Vector2D<float>::PassType point) const = delete;
	// };
	//
	//
	// struct Ty{
	// 	static_quad_tree_node<EmptyV> v{};
	// };

	constexpr std::size_t top_lft_index = 0;
	constexpr std::size_t top_rit_index = 1;
	constexpr std::size_t bot_lft_index = 2;
	constexpr std::size_t bot_rit_index = 3;

	template <typename ItemTy, ext::number T = float>
		requires std::derived_from<ItemTy, QuadTreeAdaptable<ItemTy, T>>
	struct quad_tree_node : basic_quad_tree_node<ItemTy, T>{
	public:
		using rect_type = Rect_Orthogonal<T>;
		using trait = quad_tree_evaluateable_traits<ItemTy, T>;

		using basic_quad_tree_node<ItemTy, T>::basic_quad_tree_node;

	private:
		bool withinBound(const ItemTy& object, const T dst) const noexcept{
			return trait::bound_of(object).getCenter().within(this->boundary.getCenter(), dst);
		}

		struct sub_nodes{
			std::array<quad_tree_node, 4> nodes{};

			[[nodiscard]] explicit sub_nodes(const std::array<rect_type, 4>& rects)
				: nodes{
					quad_tree_node{rects[bot_lft_index]},
					quad_tree_node{rects[bot_rit_index]},
					quad_tree_node{rects[top_lft_index]},
					quad_tree_node{rects[top_rit_index]},
				}{
			}

			void reserved_clear() noexcept{
				for (auto& node : nodes){
					node.reserved_clear();
				}
			}

			constexpr auto begin() noexcept{
				return nodes.begin();
			}

			constexpr auto end() noexcept{
				return nodes.end();
			}

			constexpr quad_tree_node& at(const unsigned i) noexcept{
				return nodes[i];
			}

		};

		std::unique_ptr<sub_nodes> children{};

		[[nodiscard]] bool is_children_cached() const noexcept{
			return children != nullptr;
		}

		void internalInsert(ItemTy* item){
			this->items.push_back(item);
			++this->branch_size;
		}

		void split(){
			if(!std::exchange(this->leaf, false)) return;

			if(!is_children_cached()){
				children = std::make_unique<sub_nodes>(this->boundary.split());
			}

			std::erase_if(this->items, [this](ItemTy* item){
				if(quad_tree_node* sub = this->get_wrappable_child(trait::bound_of(*item))){
					sub->internalInsert(item);
					return true;
				}

				return false;
			});
		}

		void unsplit() noexcept{
			if(std::exchange(this->leaf, true)) return;

			assert(is_children_cached());

			this->items.reserve(this->branch_size - this->items.size());
			for (quad_tree_node& node : children->nodes){
				this->items.append_range(node.items);
				node.reserved_clear();
			}
		}

		[[nodiscard]] quad_tree_node* get_wrappable_child(const rect_type target_bound) const noexcept{
			assert(!this->is_leaf());
			assert(is_children_cached());

			auto [midX, midY] = this->boundary.getCenter();

			// Object can completely fit within the top quadrants
			const bool topQuadrant = target_bound.getSrcY() > midY;
			// Object can completely fit within the bottom quadrants
			const bool bottomQuadrant = target_bound.getEndY() < midY;

			// Object can completely fit within the left quadrants
			if(target_bound.getEndX() < midX){
				if(topQuadrant){
					return &children->at(top_lft_index);
				}

				if(bottomQuadrant){
					return &children->at(bot_lft_index);
				}
			} else if(target_bound.getSrcX() > midX){
				// Object can completely fit within the right quadrants
				if(topQuadrant){
					return &children->at(top_rit_index);
				}

				if(bottomQuadrant){
					return &children->at(bot_rit_index);
				}
			}

			return nullptr;
		}

	public:
		void reserved_clear() noexcept{
			if(std::exchange(this->branch_size, 0) == 0) return;

			if(!this->leaf){
				children->reserved_clear();
				this->leaf = true;
			}

			this->items.clear();
		}

		void clear() noexcept{
			children.reset(nullptr);

			this->leaf = true;
			this->branch_size = 0;

			this->items.clear();
		}

	private:
		void insertImpl(ItemTy& box){
			//If this box is inbound, it must be added.
			++this->branch_size;

			// Otherwise, subdivide and then add the object to whichever node will accept it
			if(this->leaf){
				if(this->items.size() < MaximumItemCount){
					this->items.push_back(std::addressof(box));
					return;
				}

				split();
			}

			const rect_type rect = trait::bound_of(box);

			if(quad_tree_node* child = this->get_wrappable_child(rect)){
				child->insertImpl(box);
				return;
			}

			if(this->items.size() < MaximumItemCount * 2){
				this->items.push_back(std::addressof(box));
			}else{
				const auto [cx, cy] = rect.getCenter();
				const auto [bx, by] = this->boundary.getCenter();

				const bool left = cx < bx;
				const bool bottom = cy < by;

				if(bottom){
					if(left){
						children->at(bot_lft_index).insertImpl(box);
					}else{
						children->at(bot_rit_index).insertImpl(box);
					}
				}else{
					if(left){
						children->at(top_lft_index).insertImpl(box);
					}else{
						children->at(top_rit_index).insertImpl(box);
					}
				}

			}
		}

	public:
		bool insert(ItemTy& box){
			// Ignore objects that do not belong in this quad tree
			if(!this->overlaps(box)){
				return false;
			}

			this->insertImpl(box);
			return true;
		}

		bool remove(const ItemTy& box) noexcept{
			if(this->branch_size == 0) return false;

			bool result;
			if(this->is_leaf()){
				result = static_cast<bool>(std::erase_if(this->items, [ptr = std::addressof(box)](const ItemTy* o){
					return o == ptr;
				}));
			} else{
				if(quad_tree_node* child = this->get_wrappable_child(trait::bound_of(box))){
					result = child->remove(box);
				} else{
					result = static_cast<bool>(std::erase_if(this->items, [ptr = std::addressof(box)](const ItemTy* o){
						return o == ptr;
					}));
				}
			}

			if(result){
				--this->branch_size;
				if(this->branch_size < MaximumItemCount) unsplit();
			}

			return result;
		}

		// ------------------------------------------------------------------------------
		// external usage
		// ------------------------------------------------------------------------------

		template <std::regular_invocable<ItemTy&> Func>
		void each(Func func) const{
			for(auto item : this->items){
				std::invoke(func, *item);
			}

			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.each(func);
				}
			}
		}

		template <std::regular_invocable<quad_tree_node&> Func>
		void each(Func func) const{
			std::invoke(func, *this);

			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.each(func);
				}
			}
		}


		[[nodiscard]] ItemTy* intersect_any(const ItemTy& object) const noexcept{
			if(!this->overlaps(object)){
				return nullptr;
			}

			for(const auto item : this->items){
				if(trait::isIntersectedBetween(object, *item)){
					return item;
				}
			}

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					if(auto rst = node.intersect_any(object)) return rst;
				}
			}

			// Otherwise, the rectangle does not overlap with any rectangle in the quad tree
			return nullptr;
		}

		[[nodiscard]] ItemTy* intersect_any(const Vec2 point) const noexcept requires (trait::HasPointIntersect){
			if(!this->overlaps(point)){
				return nullptr;
			}

			for(const auto item : this->items){
				if(trait::isIntesectedWithPoint(point, *item)){
					return item;
				}
			}


			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					if(auto rst = node.intersect_any(point)) return rst;
				}
			}

			return nullptr;
		}

		template <
			std::invocable<ItemTy&, ItemTy&> Func,
			std::predicate<const ItemTy&, const ItemTy&> Filter = always_true>
		void intersect_test_all(ItemTy& object, Func func, Filter filter = {}) const{
			if(!this->overlaps(object)) return;

			for(const auto element : this->items){
				if(trait::isIntersectedBetween(object, *element)){
					if(std::invoke(filter, object, *element)){
						std::invoke(func, *element, object);
					}
				}
			}

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.intersect_test_all(object, func, filter);
				}
			}
		}

		template <
			std::invocable<const ItemTy&, const ItemTy&> Func,
			std::predicate<const ItemTy&, const ItemTy&> Filter = always_true>
		void intersect_test_all(const ItemTy& object, Func func, Filter filter = {}) const{
			if(!this->overlaps(object)) return;

			for(const auto element : this->items){
				if(trait::isIntersectedBetween(object, *element)){
					if(std::invoke(filter, object, *element)){
						std::invoke(func, *element, object);
					}
				}
			}

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.intersect_test_all(object, func, filter);
				}
			}
		}

	private:
		template <std::predicate<const ItemTy&, const ItemTy&> Filter>
		void get_all_intersected_impl(const ItemTy& object, Filter filter, std::vector<ItemTy*>& out) const{
			if(!this->overlaps(object)) return;

			for(const auto element : this->items){
				if(trait::isIntersectedBetween(object, *element)){
					if(std::invoke(filter, object, *element)){
						out.push_back(element);
					}
				}
			}

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.get_all_intersected_impl(object, filter, out);
				}
			}
		}

	public:
		template <std::predicate<const ItemTy&, const ItemTy&> Filter>
		[[nodiscard]] std::vector<ItemTy*> get_all_intersected(const ItemTy& object, Filter filter) const{
			std::vector<ItemTy*> rst{};
			rst.reserve(this->branch_size / 4);

			this->get_all_intersected_impl<Filter>(object, filter, rst);
			return rst;
		}

		template <std::invocable<ItemTy&> Func>
		void intersect_then(const ItemTy& object, Func func) const{
			if(!this->overlaps(object)) return;

			for(auto* cont : this->items){
				if(trait::isIntersectedBetween(object, *cont)){
					std::invoke(func, *cont);
				}
			}

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.intersect_then(object, func);
				}
			}

		}

		template <std::invocable<ItemTy&, rect_type> Func>
		void intersect_then(const rect_type rect, Func func) const{
			if(!this->overlaps(rect)) return;

			//If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.intersect_then(rect, func);
				}
			}

			for(auto* cont : this->items){
				if(trait::bound_of(*cont).overlap_Exclusive(rect)){
					std::invoke(func, *cont, rect);
				}
			}
		}

		template <typename Region, std::predicate<const rect_type&, const Region&> Pred, std::invocable<ItemTy&, const Region&> Func>
			requires !std::same_as<Region, rect_type>
		void intersect_then(const Region& region, Pred boundCheck, Func func) const{
			if(!std::invoke(boundCheck, this->boundary, region)) return;

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.template intersect_then<Region>(region, boundCheck, func);
				}
			}

			for(auto cont : this->items){
				if(std::invoke(boundCheck, trait::bound_of(*cont), region)){
					std::invoke(func, *cont, region);
				}
			}
		}

		template <std::regular_invocable<ItemTy&, Vec2> Func>
		void intersect_then(const Vec2 point, Func func) const{
			if(!this->overlaps(point)) return;

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.intersect_then(point, func);
				}
			}

			for(auto cont : this->items){
				if(trait::isIntesectedWithPoint(point, *cont)){
					std::invoke(func, *cont, point);
				}
			}
		}

		template <std::invocable<ItemTy&> Func>
		void within(const ItemTy& object, const T dst, Func func) const{
			if(!this->withinBound(object, dst)) return;

			// If this node has children, check if the rectangle overlaps with any rectangle in the children
			if(this->has_valid_children()){
				for (const quad_tree_node & node : children->nodes){
					node.within(object, dst, func);
				}
			}

			for(const auto* cont : this->items){
				std::invoke(func, *cont);
			}
		}
	};

	export
	template <typename ItemTy, ext::number T = float>
		requires ext::derived<ItemTy, QuadTreeAdaptable<ItemTy, T>>
	using quad_tree = quad_tree_node<ItemTy, T>;

	export
	template <typename ItemTy, ext::number T = typename ItemTy::coordinate_type>
		requires ext::derived<ItemTy, QuadTreeAdaptable<ItemTy, T>>
	struct quad_tree_with_buffer : quad_tree_node<ItemTy, T>{
	private:
		using buffer = std::vector<ItemTy*>;
		buffer toInsert{};
		buffer toRemove{};

		std::mutex insertMutex{};
		std::mutex removeMutex{};
		std::mutex consumeMutex{};

	public:
		void push_insert(ItemTy& item) noexcept{
			std::lock_guard guard{insertMutex};
			toInsert.push_back(item);
		}

		void push_remove(ItemTy& item) noexcept{
			std::lock_guard guard{removeMutex};
			toRemove.push_back(item);
		}

		void consume_buffer(){
			buffer inserts, removes;

			{
				std::lock_guard guard{insertMutex};
				inserts = std::move(toInsert);
			}

			{
				std::lock_guard guard{removeMutex};
				removes = std::move(toRemove);
			}

			std::lock_guard guard{consumeMutex};

			for(auto val : inserts){
				quad_tree_node<ItemTy, T>::insert(*val);
			}

			for(auto val : removes){
				quad_tree_node<ItemTy, T>::remove(*val);
			}
		}


		/**
		 * @warning Call this function if guarantees to be thread safe
		 */
		void consume_buffer_guaranteed(){
			for(auto val : toInsert){
				quad_tree_node<ItemTy, T>::insert(*val);
			}

			for(auto val : toRemove){
				quad_tree_node<ItemTy, T>::remove(*val);
			}


			toInsert.clear();
			toRemove.clear();
		}
	};

	//TODO quad tree with thread safe insert / remove buffer
}
