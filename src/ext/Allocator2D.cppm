export module ext.Allocator2D;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import std;

export namespace ext {
    class Allocator2D{
        using T = std::uint32_t;
        using SizeType = Geom::Vector2D<T>;
        using PointType = Geom::Vector2D<T>;
        using Rect = Geom::Rect_Orthogonal<T>;
        using SubRectArr = std::array<Rect, 3>;

        SizeType size{};
        T remainArea{};

        class Node;

        std::multimap<T, Node*> valid_widthAscend{};
        std::multimap<T, Node*> valid_heightAscend{};

        std::unique_ptr<Node> rootNode{std::make_unique<Node>(this)};
        std::unordered_map<PointType, Node*> allocatedNodes{};

        using Itr = decltype(valid_widthAscend)::iterator;

        class Node{
            friend Allocator2D;
            Allocator2D* packer{};
            Node* parent{};
            std::array<std::unique_ptr<Node>, 3> subNodes{};

            Rect bound{};
            Rect allocatedRegion{};

            Itr xItr{};
            Itr yItr{};

            bool allocated{};
            bool childrenCleared{true};

        public:
            [[nodiscard]] Node() = default;

            [[nodiscard]] explicit Node(Allocator2D* packer) :
                packer{packer} {}

            [[nodiscard]] Node(Allocator2D* packer, Node* parent, const Rect& bound) :
                packer{packer},
                parent{parent},
                bound{bound} {
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

                if(isLeaf()) {
                    tryUnsplit();
                }else {
                    if(!unsplit()) {
                        //Can only remove self, mark allocatable
                        insertSelf(true);
                    }
                }

                allocatedRegion = {};
                return size;
            }

        private:
            void split() {
                for (const auto & [i, rect] : splitQuad(bound, allocatedRegion) | std::views::enumerate) {
                    if(rect.area() == 0)continue;
                    subNodes[i] = std::make_unique<Node>(Node{packer,this, rect});
                    subNodes[i]->insertWithHint(xItr, yItr);
                }
            }

            void tryUnsplit(){ // NOLINT(*-no-recursion)
                if(!isRoot()) {
                    const auto rst = parent->unsplit();
                    if(!rst){ //maximum merge at self, mark allocatable
                        eraseSelf();
                        insertSelf(false);
                    }
                }else {
                    eraseSelf();
                    insertSelf(false);
                }
            }

            bool unsplit() { // NOLINT(*-no-recursion)
                if(isBranchEmpty()) {
                    childrenCleared = true;
                    for (auto && subNode : subNodes) {
                        if(subNode == nullptr)continue;
                        subNode->eraseSelf();
                        subNode.reset();
                    }

                    tryUnsplit();

                    return true;
                }

                return false;
            }

            void insertWithHint(const Itr& hintX, const Itr& hintY) {
                xItr = packer->valid_widthAscend.emplace_hint(hintX, bound.getWidth(), this);
                yItr = packer->valid_heightAscend.emplace_hint(hintY, bound.getHeight(), this);
            }

            void eraseSelf() {
                if(xItr != packer->valid_widthAscend.end())packer->valid_widthAscend.erase(xItr);
                if(yItr != packer->valid_heightAscend.end())packer->valid_heightAscend.erase(yItr);
                xItr = packer->valid_widthAscend.end();
                yItr = packer->valid_heightAscend.end();
            }

            void insertSelf(const bool suppressed) {
                const Rect& bound = suppressed ? this->allocatedRegion : this->bound;
                xItr = packer->valid_widthAscend.emplace(bound.getWidth(), this);
                yItr = packer->valid_heightAscend.emplace(bound.getHeight(), this);
            }
        };
        friend Node;

    public:
        [[nodiscard]] explicit Allocator2D(const SizeType size) : size{size}, remainArea{size.area()} {
            rootNode->bound.setSize(size);
            rootNode->insertSelf(false);
        }

        [[nodiscard]] std::optional<Rect> allocate(const SizeType size) {
            if(size.area() == 0)throw std::invalid_argument("Cannot allocate region with 0 area");

            if(remainArea < size.area()) {return std::nullopt;}

            const Itr xItr = valid_widthAscend.lower_bound(size.x);
            const Itr yItr = valid_heightAscend.lower_bound(size.y);

            Node* node{};

            for(const auto xNode : std::ranges::subrange{xItr, valid_widthAscend.end()} | std::views::values) {
                for(const auto yNode : std::ranges::subrange{yItr, valid_heightAscend.end()} | std::views::values) {
                    if(xNode->bound.getSrc() == yNode->bound.getSrc()) {

#if DEBUG_CHECK
                        if(xNode != yNode) {
                            throw std::runtime_error("Assertion Failure");
                        }
#endif

                        node = xNode;
                        break;
                    }
                }
            }

            if(!node)return std::nullopt;

            remainArea -= size.area();

            return node->allocate(size);
        }

        void deallocate(const PointType src) noexcept {
            if(const auto itr = allocatedNodes.find(src); itr != allocatedNodes.end()) {
                remainArea += itr->second->deallocate().area();
            }
        }

        Allocator2D(const Allocator2D& other) = delete;

        Allocator2D& operator=(const Allocator2D& other) = delete;

        Allocator2D(Allocator2D&& other) noexcept
            : size{std::move(other.size)},
              valid_widthAscend{std::move(other.valid_widthAscend)},
              valid_heightAscend{std::move(other.valid_heightAscend)},
              rootNode{std::move(other.rootNode)},
              allocatedNodes{std::move(other.allocatedNodes)}{

            changePackerToThis();
        }

        Allocator2D& operator=(Allocator2D&& other) noexcept{
            if(this == &other) return *this;
            size = std::move(other.size);
            valid_widthAscend = std::move(other.valid_widthAscend);
            valid_heightAscend = std::move(other.valid_heightAscend);
            rootNode = std::move(other.rootNode);
            allocatedNodes = std::move(other.allocatedNodes);
            changePackerToThis();

            return *this;
        }

    private:
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