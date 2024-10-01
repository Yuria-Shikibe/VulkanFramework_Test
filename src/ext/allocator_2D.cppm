module;

#include <cassert>

export module ext.allocator_2D;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import std;

namespace ext {
    struct allocator_2D_base{
    protected:
        using T = std::uint32_t;

    public:
        using size_type = T;
        using extent_type = Geom::Vector2D<T>;
        using point_type = Geom::Vector2D<T>;
        using rect_type = Geom::Rect_Orthogonal<T>;
        using sub_region_type = std::array<rect_type, 3>;

    protected:
        extent_type size{};
        T remainArea{};

        struct node;

        using tree_type = std::map<T, std::multimap<T, node*>>;

        tree_type nodes_XY{};
        tree_type nodes_YX{};

        std::unique_ptr<node> rootNode{std::make_unique<node>(this)};

        std::unordered_map<point_type, node*> allocatedNodes{};

        using ItrOuter = tree_type::iterator;
        using ItrInner = tree_type::mapped_type::iterator;

        struct ItrPair{
            ItrOuter outer{};
            ItrInner inner{};

            [[nodiscard]] auto* value() const{
                return inner->second;
            }

            void erase(tree_type& tree) const{
                auto itr = outer->second.erase(inner);
                if(outer->second.empty()){
                    tree.erase(outer);
                }
            }

            void insert(tree_type& tree, node* node, const T first, const T second){
                outer = tree.try_emplace(first).first;
                inner = outer->second.insert({second, node});
            }

            void locate(tree_type& tree, const T first, const T second){
                outer = tree.lower_bound(first);
                inner = outer->second.lower_bound(second);
            }

            /**
             * @return [rst, possible to find]
             */
            void locateNextInner(const T second){
                inner = outer->second.lower_bound(second);
            }

            bool locateNextOuter(tree_type& tree){
                ++outer;
                if(outer == tree.end()) return false;
                return true;
            }

            [[nodiscard]] bool valid(const tree_type& tree) const noexcept{
                return outer != tree.end() && inner != outer->second.end();
            }
        };

        struct node{
            allocator_2D_base* packer{};
            node* parent{};

            std::array<std::unique_ptr<node>, 3> subNodes{};

            rect_type capacityRegion{};
            extent_type capturedExtent{};

            ItrPair itrXY{};
            ItrPair itrYX{};

            bool allocated{};
            bool marked{};
            bool childrenCleared{true};

            [[nodiscard]] node() noexcept = default;

            [[nodiscard]] explicit node(allocator_2D_base* packer) noexcept :
                packer{packer}{}

            [[nodiscard]] node(allocator_2D_base* packer, node* parent, const rect_type bound) noexcept :
                packer{packer},
                parent{parent},
                capacityRegion{bound}{}

            [[nodiscard]] bool isRoot() const noexcept{
                return parent == nullptr;
            }

            [[nodiscard]] bool isLeaf() const noexcept{
                return subNodes[0] == nullptr && subNodes[1] == nullptr && subNodes[2] == nullptr;
            }

            [[nodiscard]] bool full() const noexcept{
                return capacityRegion.getSize() == capturedExtent;
            }

            [[nodiscard]] bool empty() const noexcept{
                return capturedExtent.area() == 0;
            }

            [[nodiscard]] bool isAllocated() const noexcept{ return allocated; }

            [[nodiscard]] bool isBranchEmpty() const noexcept{
                if(isAllocated()) return false;

                return std::ranges::all_of(subNodes, [](const std::unique_ptr<node>& node){
                    return node == nullptr || (!node->isAllocated() && node->childrenCleared);
                });
            }

            [[nodiscard]] rect_type allocate(const extent_type size){
                if(size.x > capacityRegion.getWidth() || size.y > capacityRegion.getHeight()){
                    throw std::bad_alloc{};
                }

                allocated = true;
                capturedExtent = size;
                packer->allocatedNodes.insert_or_assign(capacityRegion.getSrc(), this);

                if(isLeaf() && !full()){
                    split();
                }

                eraseMark();

                if(!isRoot()){
                    parent->childrenCleared = false;
                }

                return {Geom::FromExtent, capacityRegion.src, size};
            }

            extent_type deallocate(){
                allocated = false;
                packer->allocatedNodes.erase(capacityRegion.getSrc());
                const auto size = capturedExtent;

                //warning: this may have been destructed after this function call
                unsplit(size);

                return size;
            }

            void split(){
                for(const auto& [i, rect] : splitQuad(capacityRegion, capturedExtent) | std::views::enumerate){
                    if(rect.area() == 0) continue;

                    subNodes[i] = std::make_unique<node>(packer, this, rect);
                    subNodes[i]->addMark(rect.getSize());
                }
            }

            void unsplit(const extent_type size){
                if(isLeaf()){
                    callParentUnsplit();
                } else{
                    if(!tryUnsplit()){
                        //Can only remove self, mark allocatable
                        addMark(size);
                    }
                }
            }

            void callParentUnsplit(){ // NOLINT(*-no-recursion)
                if(!isRoot()){
                    const auto isSelfMerged = parent->tryUnsplit();
                    if(!isSelfMerged){
                        //maximum merge at self, mark allocatable
                        resetMark(capacityRegion.getSize());
                    }
                } else{
                    resetMark(capacityRegion.getSize());
                }
            }

            /**
             * @brief
             * @return true if merge is successful
             */
            bool tryUnsplit(){ // NOLINT(*-no-recursion)
                if(isBranchEmpty()){
                    childrenCleared = true;
                    for(auto&& subNode : subNodes){
                        if(subNode == nullptr) continue;
                        subNode->eraseMark();
                        subNode.reset();
                    }

                    callParentUnsplit();

                    return true;
                }

                return false;
            }

            void eraseMark(){
                if(!marked) return;
                itrXY.erase(packer->nodes_XY);
                itrYX.erase(packer->nodes_YX);

                marked = false;
            }

            void addMark(const extent_type size){
                itrXY.insert(packer->nodes_XY, this, size.x, size.y);
                itrYX.insert(packer->nodes_YX, this, size.y, size.x);
                marked = true;
            }

            void resetMark(const extent_type size){
                if(!marked) return;
                itrXY.erase(packer->nodes_XY);
                itrYX.erase(packer->nodes_YX);

                itrXY.insert(packer->nodes_XY, this, size.x, size.y);
                itrYX.insert(packer->nodes_YX, this, size.y, size.x);
            }
        };

        friend node;

        /**
         * @code
         *    array[0]  |  array[2]
         * -------------|------------
         *      Box     |  array[1]
         * @endcode
         */
        [[nodiscard]] static constexpr sub_region_type splitQuad(const rect_type bound, const extent_type box) noexcept{
            const auto [rx, ry] = bound.getSize() - box;

            return sub_region_type{
                rect_type{Geom::FromExtent, bound.vert_00().add(0, box.y), box.x, ry},
                rect_type{Geom::FromExtent, bound.vert_00().add(box.x, 0), rx, box.y},
                rect_type{Geom::FromExtent, bound.vert_00() + box, rx, ry},
            };
        }

    public:
        [[nodiscard]] allocator_2D_base() = default;

        [[nodiscard]] explicit allocator_2D_base(const extent_type size) : size{size}, remainArea{size.area()} {
            rootNode->capacityRegion.setSize(size);
            rootNode->addMark(size);
        }
    };


    export class allocator_2D : public allocator_2D_base{
    public:
        using allocator_2D_base::allocator_2D_base;

        //TODO double key -- with allocation region depth
        [[nodiscard]] std::optional<rect_type> allocate(const extent_type size) {
            auto* node = getValidNode(size);

            if(!node)return std::nullopt;

            remainArea -= size.area();

            return node->allocate(size);
        }

        void deallocate(const point_type src) noexcept {
            if(const auto itr = allocatedNodes.find(src); itr != allocatedNodes.end()) {
                remainArea += itr->second->deallocate().area();
            }
        }

        [[nodiscard]] extent_type extent() const noexcept{ return size; }

        [[nodiscard]] T get_valid_area() const noexcept{ return remainArea; }

        allocator_2D(const allocator_2D& other) = delete;

        allocator_2D(allocator_2D&& other) noexcept
            : allocator_2D_base{std::move(other)}{
            changePackerToThis();
        }

        allocator_2D& operator=(const allocator_2D& other) = delete;

        allocator_2D& operator=(allocator_2D&& other) noexcept{
            if(this == &other) return *this;
            allocator_2D_base::operator =(std::move(other));
            changePackerToThis();
            return *this;
        }

    private:
        node* getValidNode(const extent_type size){
            assert(size.area() > 0);

            if(remainArea < size.area()) {return nullptr;}

            ItrPair itrPairXY{nodes_XY.lower_bound(size.x)};
            ItrPair itrPairYX{nodes_YX.lower_bound(size.y)};

            node* node{};

            bool possibleX{itrPairXY.outer != nodes_XY.end()};
            bool possibleY{itrPairYX.outer != nodes_YX.end()};

            while(true)
            {
                if(!possibleX && !possibleY)break;

                if(possibleX){
                    itrPairXY.locateNextInner(size.y);
                    if (itrPairXY.valid(nodes_XY)){
                        node = itrPairXY.value();
                        break;
                    }else{
                        possibleX = itrPairXY.locateNextOuter(nodes_XY);
                    }
                }

                if(possibleY){
                    itrPairYX.locateNextInner(size.x);
                    if (itrPairYX.valid(nodes_YX)){
                        node = itrPairYX.value();
                        break;
                    }else{
                        possibleY = itrPairYX.locateNextOuter(nodes_YX);
                    }
                }
            }

            return node;
        }

        template <std::regular_invocable<allocator_2D&, node&> Fn>
        void dfsEach_impl(Fn fn, node* current){
            std::invoke(fn, *this, *current);

            for (auto && subNode : current->subNodes){
                if(subNode){
                    this->dfsEach_impl<Fn>(fn, subNode.get());
                }
            }
        }

        template <std::regular_invocable<allocator_2D&, node&> Fn>
        void dfsEach(Fn fn){
            this->dfsEach_impl<Fn>(std::move(fn), rootNode.get());
        }

        void changePackerToThis(){
            dfsEach([](allocator_2D& packer, node& node){
                node.packer = &packer;
            });
        }
    };

}