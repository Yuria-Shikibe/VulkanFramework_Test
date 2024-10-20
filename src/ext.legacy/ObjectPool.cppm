export module ext.Container.ObjectPool;

import std;

import ext.concepts;
import ext.RuntimeException;
import ext.owner;


export namespace ext{
    //TODO this class is a totoally failure
    //TODO uses allocator
    template <class T, typename Alloc = std::allocator<T>>
        requires ext::DefConstructable<T>
    class ObjectPool{
    public:
        struct Deleter {
            ObjectPool* src{ nullptr };

            ~Deleter() = default;

            Deleter() = default;

            explicit Deleter(ObjectPool* src)
                : src(src) {
            }

            void operator()(const ext::owner<T*> t) const {
                if(!src) {
                    // if constexpr (requires{
                    //     Alloc{};
                    // }){
                    //     Alloc alloc;
                    //     alloc.deallocate(t, 1);
                    // }else{
                    //
                    // }

                    t->~T();
                    std::free(t);
                }else{
                    src->store(t);
                }
            }

            friend bool operator==(const Deleter& lhs, const Deleter& rhs){ return lhs.src == rhs.src; }

            friend bool operator!=(const Deleter& lhs, const Deleter& rhs){ return !(lhs == rhs); }
        };

        struct Allocator_Raw : ::std::allocator<ext::owner<T*>> {
            ObjectPool* const src{ nullptr };

            ext::owner<T*>* allocate(const size_t count) {
                return &src->obtainRaw();
            }

            void deallocate(ext::owner<T*>* ptr, size_t n) {
                src->store(*ptr);
            }
        };

        struct Allocator_Shared : ::std::allocator<std::shared_ptr<T>> {
            ObjectPool* const src{ nullptr };

            std::shared_ptr<T>* allocate(const size_t count) {
                return &src->obtainShared();
            }

            void deallocate(std::shared_ptr<T>* ptr, size_t n) {
                ptr->reset(nullptr);
            }
        };

        struct Allocator_Unique : ::std::allocator<std::unique_ptr<T>> {
            ObjectPool* const src{ nullptr };

            std::unique_ptr<T>* allocate(const size_t count) {
                return &src->obtainUnique();
            }

            void deallocate(std::unique_ptr<T>* ptr, size_t n) {
                src->store(std::move(*ptr));
            }
        };

    protected:
        Alloc alloc{};
        std::size_t maxSize{5000};
        std::vector<ext::owner<T*>> vault;

        Deleter deleter{this};
        std::mutex vaultLock{};

    public:
        Deleter getDeleter(){
            return deleter;
        }

        using UniquePtr = std::unique_ptr<T, Deleter>;

        explicit ObjectPool(const size_t maxSize) : maxSize(maxSize){
            vault.reserve(maxSize);
        }

        ObjectPool(const ObjectPool& other) = delete;

        ObjectPool& operator=(const ObjectPool& other) = delete;

        ObjectPool(ObjectPool&& other) noexcept = default;

        ObjectPool& operator=(ObjectPool&& other) noexcept = default;

        ObjectPool(){
            vault.reserve(maxSize);
        }

        ~ObjectPool() {
            for(auto element : vault){
                std::free(element);
            }
        }

        void store(std::shared_ptr<T>& ptr) {
            //TODO does this safe>>
            if(std::addressof(*std::get_deleter<Deleter>(ptr)) == deleter) {
                ptr.reset(nullptr);
            }
        }

        void store(std::unique_ptr<T>&& ptr){
            ptr.get()->~T();
            this->store(ptr.release());
        }

        void store(ext::owner<T*> ptr) {
            ptr->~T();

            if(std::lock_guard guard{vaultLock}; vault.size() < maxSize){
                vault.push_back(ptr);
                return;
            }

            std::free(ptr);
        }

        // std::pmr::monotonic_buffer_resource* getMonotonicPool() {
        //     if(monotonicPool) {
        //         return monotonicPool.get();
        //     }
        //
        //     monotonicPool = std::make_unique<std::pmr::monotonic_buffer_resource>(vault.data(), vault.capacity());
        //     return monotonicPool.get();
        // }

        Allocator_Raw getAllocator_Raw() const {
            return Allocator_Raw{this};
        }

        Allocator_Shared getAllocator_Shared() const {
            return Allocator_Shared{this};
        }

        Allocator_Unique getAllocator_Unique() const {
            return Allocator_Unique{this};
        }

        [[nodiscard]] UniquePtr obtainUnique() {
            return UniquePtr{obtainRaw(), deleter};
        }

        [[nodiscard]] ext::owner<T*> obtainRaw() {
            ext::owner<T*> ptr{nullptr};
            if (std::lock_guard guard{vaultLock}; !vault.empty()) {
                ptr = vault.back();
                vault.pop_back();
            }

            if(!ptr){
                ptr = static_cast<ext::owner<T*>>(std::malloc(sizeof T));
            }

            new (ptr) T{};

            return ptr;
        }


        [[nodiscard]] std::shared_ptr<T> obtainShared() {
            return std::shared_ptr<T>{obtainRaw(), deleter};
        }

        [[nodiscard]] size_t size() const{
            std::lock_guard guard{vaultLock};
            return vault.size();
        }

        void clear(){
            std::lock_guard guard{vaultLock};
            vault.clear();
        }
    };
}

