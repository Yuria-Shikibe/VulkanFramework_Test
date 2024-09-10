export module ext.Allocator2D;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import std;

export namespace ext {
    class allocator_2D{
        using T = std::uint32_t;
        using SizeType = Geom::Vector2D<T>;
        using PointType = Geom::Vector2D<T>;
        using Rect = Geom::Rect_Orthogonal<T>;
        using SubRectArr = std::array<Rect, 3>;

        SizeType size{};
        T remainArea{};

        struct Node;

        using TreeType = std::map<T, std::multimap<T, Node*>>;

        TreeType nodes_XY{};
        TreeType nodes_YX{};

        std::unique_ptr<Node> rootNode{std::make_unique<Node>(this)};

        std::unordered_map<PointType, Node*> allocatedNodes{};

        using ItrOuter = TreeType::iterator;
        using ItrInner = TreeType::mapped_type::iterator;

        struct ItrPair{
            ItrOuter outer{};
            ItrInner inner{};

            [[nodiscard]] auto* value() const{
                return inner->second;
            }

            void erase(TreeType& tree) const{
                auto itr = outer->second.erase(inner);
                if(outer->second.empty()){
                    tree.erase(outer);
                }
            }

            void insert(TreeType& tree, Node* node, const T first, const T second){
                outer = tree.try_emplace(first).first;
                inner = outer->second.insert({second, node});
            }

            void locate(TreeType& tree, const T first, const T second){
                outer = tree.lower_bound(first);
                inner = outer->second.lower_bound(second);
            }

            /**
             * @return [rst, possible to find]
             */
            void locateNextInner(const T second){
                inner = outer->second.lower_bound(second);
            }

            bool locateNextOuter(TreeType& tree){
                ++outer;
                if(outer == tree.end())return false;
                return true;
            }

            [[nodiscard]] bool valid(const TreeType& tree) const noexcept{
                return outer != tree.end() && inner != outer->second.end();
            }
        };

        struct Node{
            allocator_2D* packer{};
            Node* parent{};
            std::array<std::unique_ptr<Node>, 3> subNodes{};

            Rect bound{};
            Rect allocatedRegion{};

            ItrPair itrXY{};
            ItrPair itrYX{};

            bool allocated{};
            bool marked{};
            bool childrenCleared{true};

            [[nodiscard]] Node() = default;

            [[nodiscard]] explicit Node(allocator_2D* packer) :
                packer{packer} {}

            [[nodiscard]] Node(allocator_2D* packer, Node* parent, const Rect& bound) :
                packer{packer},
                parent{parent},
                bound{bound}
            {
            }

            [[nodiscard]] bool isRoot() const noexcept{
                return parent == nullptr;
            }

            [[nodiscard]] bool isLeaf() const noexcept {
                return subNodes[0] == nullptr && subNodes[1] == nullptr && subNodes[2] == nullptr;
            }

            [[nodiscard]] bool isFull() const noexcept {
                return bound == allocatedRegion;
            }

            [[nodiscard]] bool empty() const noexcept {
                return allocatedRegion.area() == 0;
            }

            [[nodiscard]] bool isAllocated() const noexcept {return allocated;}

            [[nodiscard]] std::size_t getID() const noexcept {
                const auto src = bound.getSrc();

                if(src.x == parent->bound.getSrcX())return 0;
                if(src.y == parent->bound.getSrcY())return 1;
                return 2;
            }

            [[nodiscard]] bool isBranchEmpty() {
                if(isAllocated())return false;

                if(isLeaf())return true;

                return std::ranges::all_of(subNodes, [](const std::unique_ptr<Node>& node) {
                   return node == nullptr || (!node->isAllocated() && node->childrenCleared);
                });
            }

            Rect allocate(const SizeType size) {
                if(size.x > bound.getWidth() || size.y > bound.getHeight()){
                    throw std::invalid_argument("Box size overflow");
                }

                allocated = true;
                allocatedRegion.setSize(size);
                allocatedRegion.setSrc(bound.getSrc());
                packer->allocatedNodes.insert_or_assign(bound.getSrc(), this);

                if(isLeaf() && !isFull()) {
                    split();
                }

                eraseSelf();

                if(!isRoot()) {
                    parent->childrenCleared = false;
                }


                return allocatedRegion;
            }

            SizeType deallocate() {
                allocated = false;
                packer->allocatedNodes.erase(bound.getSrc());
                const auto size = allocatedRegion.getSize();
                allocatedRegion = {};

                unsplit(size);

                return size;
            }

            void split() {
                for (const auto & [i, rect] : splitQuad(bound, allocatedRegion) | std::views::enumerate) {
                    if(rect.area() == 0)continue;

                    subNodes[i] = std::make_unique<Node>(Node{packer,this, rect});
                    subNodes[i]->insertSelf(rect.getSize());
                }
            }

            void unsplit(SizeType size){
                if(isLeaf()) {
                    callParentSplit();
                }else {
                    if(!tryMergeSelf()) {
                        //Can only remove self, mark allocatable
                        insertSelf(size);
                    }
                }
            }

            void callParentSplit(){ // NOLINT(*-no-recursion)
                if(!isRoot()) {
                    const auto rst = parent->tryMergeSelf();
                    if(!rst){ //maximum merge at self, mark allocatable
                        eraseSelf();
                        insertSelf(bound.getSize());
                    }
                }else {
                    eraseSelf();
                    insertSelf(bound.getSize());
                }
            }

            bool tryMergeSelf() { // NOLINT(*-no-recursion)
                if(isBranchEmpty()) {
                    childrenCleared = true;
                    for (auto && subNode : subNodes) {
                        if(subNode == nullptr)continue;
                        subNode->eraseSelf();
                        subNode.reset();
                    }

                    callParentSplit();

                    return true;
                }

                return false;
            }

            void eraseSelf() {
                if(!marked)return;
                itrXY.erase(packer->nodes_XY);
                itrYX.erase(packer->nodes_YX);

                marked = false;
            }

            void insertSelf(SizeType size) {
                itrXY.insert(packer->nodes_XY, this, size.x, size.y);
                itrYX.insert(packer->nodes_YX, this, size.y, size.x);
                marked = true;
            }
        };
        friend Node;

    public:
        [[nodiscard]] explicit allocator_2D(const SizeType size) : size{size}, remainArea{size.area()} {
            rootNode->bound.setSize(size);
            rootNode->insertSelf(size);
        }

        //TODO double key -- with allocation region depth
        [[nodiscard]] std::optional<Rect> allocate(const SizeType size) {
            auto* node = getValidNode(size);

            if(!node)return std::nullopt;

            remainArea -= size.area();

            return node->allocate(size);
        }

        void deallocate(const PointType src) noexcept {
            if(const auto itr = allocatedNodes.find(src); itr != allocatedNodes.end()) {
                remainArea += itr->second->deallocate().area();
            }
        }

        [[nodiscard]] SizeType getSize() const noexcept{ return size; }

        [[nodiscard]] T get_valid_area() const noexcept{ return remainArea; }

        allocator_2D(const allocator_2D& other) = delete;

        allocator_2D& operator=(const allocator_2D& other) = delete;

        allocator_2D(allocator_2D&& other) noexcept
            : size{std::move(other.size)},
              remainArea{other.remainArea},
              nodes_XY{std::move(other.nodes_XY)},
              nodes_YX{std::move(other.nodes_YX)},
              rootNode{std::move(other.rootNode)},
              allocatedNodes{std::move(other.allocatedNodes)}{
            changePackerToThis();
        }

        allocator_2D& operator=(allocator_2D&& other) noexcept{
            if(this == &other) return *this;
            size = std::move(other.size);
            remainArea = other.remainArea;
            nodes_XY = std::move(other.nodes_XY);
            nodes_YX = std::move(other.nodes_YX);
            rootNode = std::move(other.rootNode);
            allocatedNodes = std::move(other.allocatedNodes);
            changePackerToThis();
            return *this;
        }

    private:
        Node* getValidNode(const SizeType size){
            if(size.area() == 0)throw std::invalid_argument("Cannot allocate region with 0 area");

            if(remainArea < size.area()) {return nullptr;}

            ItrPair itrPairXY{nodes_XY.lower_bound(size.x)};
            ItrPair itrPairYX{nodes_YX.lower_bound(size.y)};

            Node* node{};

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
                        possibleX = possibleX && itrPairXY.locateNextOuter(nodes_XY);
                    }
                }

                if(possibleY){
                    itrPairYX.locateNextInner(size.x);
                    if (itrPairYX.valid(nodes_YX)){
                        node = itrPairYX.value();
                        break;
                    }else{
                        possibleY = possibleY && itrPairYX.locateNextOuter(nodes_YX);
                    }
                }
            }

            return node;
        }
        template <std::regular_invocable<Node&> Fn>
        void dfsEach_impl(const Fn& fn, Node* current){
            fn(*current);

            for (auto && subNode : current->subNodes){
                if(subNode){
                    this->dfsEach_impl<Fn>(fn, subNode.get());
                }
            }
        }

        template <std::regular_invocable<Node&> Fn>
        void dfsEach(Fn&& fn){
            this->dfsEach_impl(fn, rootNode.get());
        }

        void changePackerToThis(){
            dfsEach([this](Node& node){
                node.packer = this;
            });
        }



        /**
         * @code
         *    array[0]  |  array[2]
         * -------------|------------
         *      Box     |  array[1]
         * @endcode
         */
        [[nodiscard]] static constexpr SubRectArr splitQuad(const Rect& bound, const Rect& box) noexcept(!DEBUG_CHECK){
#if DEBUG_CHECK
            if(bound.getSrcX() != box.getSrcX() || bound.getSrcY() != box.getSrcY())throw std::invalid_argument{"The source of the box and the bound doesn't match"};
#endif
            return SubRectArr{
                Rect{bound.getSrcX(),   box.getEndY(),   box.getWidth()                 , bound.getHeight() - box.getHeight()},
                Rect{  box.getEndX(), bound.getSrcY(), bound.getWidth() - box.getWidth(),   box.getHeight()                  },
                Rect{  box.getEndX(),   box.getEndY(), bound.getWidth() - box.getWidth(), bound.getHeight() - box.getHeight()}
            };
        }
    };

}