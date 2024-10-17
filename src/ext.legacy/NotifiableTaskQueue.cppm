//
// Created by Matrix on 2024/6/8.
//

export module ext.TaskQueue;
import std;
import ext.concepts;

export namespace ext{
	// using FuncTy = void();
	template <typename FuncTy = void()>
	class TaskQueue{
	public:
		using TaskTy = std::packaged_task<FuncTy>;
		using RstTy = typename ext::function_traits<FuncTy>::return_type;
		using Future = std::future<RstTy>;

	private:
		std::deque<std::packaged_task<FuncTy>> tasks{};
		mutable std::mutex mtx{};

	public:
		template <ext::invocable<FuncTy> Func>
		[[nodiscard]] Future push(Func&& task){
			return this->push(std::packaged_task<FuncTy>(std::forward<Func>(task)));
		}

		[[nodiscard]] Future push(TaskTy&& task){
			Future future = task.get_future();

			{
				std::scoped_lock lk{mtx};
				tasks.push_back(std::move(task));
			}

			return future;
		}

		[[nodiscard]] std::optional<TaskTy> pop() noexcept{
			std::scoped_lock lk{mtx};

			if(tasks.empty())return std::nullopt;

			auto func = std::move(tasks.front());
			tasks.pop_front();
			return std::move(func);
		}

		[[nodiscard]] decltype(tasks) popAll() noexcept{
			std::scoped_lock lk{mtx};
			decltype(tasks) rst = std::move(tasks);
			return rst;
		}

		[[nodiscard]] bool empty() const noexcept{
			std::scoped_lock lk{mtx};
			return tasks.empty();
		}

		[[nodiscard]] auto size() const noexcept{
			std::scoped_lock lk{mtx};
			return tasks.size();
		}

		void handleAll(){
			for (auto tasks = popAll(); auto&& task : tasks){
				try{
					task.operator()();
				}catch(...){
					throw;
				}
			}
		}

		explicit operator bool() const noexcept{
			return !empty();
		}

		[[nodiscard]] TaskQueue() = default;

		TaskQueue(const TaskQueue& other) = delete;

		TaskQueue(TaskQueue&& other) noexcept{
			std::scoped_lock lk{other.mtx, mtx};
			tasks = std::move(other.tasks);
		}

		TaskQueue& operator=(const TaskQueue& other) = delete;

		TaskQueue& operator=(TaskQueue&& other) noexcept{
			if(this == &other) return *this;
			std::scoped_lock lk{other.mtx, mtx};
			tasks = std::move(other.tasks);
			return *this;
		}
	};
}
