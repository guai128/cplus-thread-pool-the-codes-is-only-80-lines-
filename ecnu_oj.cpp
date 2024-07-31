#include <iostream>
#include <vector>
#include <random>
#include <thread>
#include <semaphore>
#include <chrono>
#include <deque>
#include <functional>
#include <atomic>
using namespace std;

using Func = function<void* (void*)>;
using Call = pair<Func, void*>;

static void* sleepAndPrint(void *)
{
	auto tid = this_thread::get_id();
	random_device gen;
	int sleep_time = gen() % 9 + 1;
	printf("I' m thread %d and I will sleep for %ds\n", tid, sleep_time);
	this_thread::sleep_for(chrono::seconds(sleep_time));
	printf("I' m thread %d and I have waked\n", tid);

	return nullptr;
}
 
class ThreadPool
{
private:
	struct ThreadHandle
	{
		enum class State { Busy, Relax };
		thread t;
		State state = State::Relax;
		bool isTerminal = false;
	};

public:
	explicit ThreadPool(int n)
		: _sem_task(0), _sem_deque_rw(1), _end(false)
	{
		for (int i = 0; i < n; ++i)
			this->_threads.push_back({ std::thread(&ThreadPool::relax, this, i) });
	}

	void add_task(Call task)
	{
		this->_sem_deque_rw.acquire();
		this->_tasks.push_back(task);
		this->_sem_deque_rw.release();

		this->_sem_task.release();
	}

	bool isFinish() 
	{
		this->_sem_deque_rw.acquire();
		bool is_task_empty = this->_tasks.empty();
		this->_sem_deque_rw.release();
		if (! is_task_empty) return false;	

		// judge weather all threads are relaxing
		for (auto& t_handle : this->_threads)
			if (!t_handle.isTerminal && t_handle.state == ThreadHandle::State::Busy) return false;
		return true;
	}

	void terminalAll()
	{
		this->_end.store(true);
		this->_sem_task.release(this->_threads.size());

		for (auto& t_handle : this->_threads)
			t_handle.t.join();
	}
private:
	void relax(int index)
	{
		while (true)
		{
			_sem_task.acquire();
			if (this->_end.load()) break;

			this->_sem_deque_rw.acquire();
			auto call = _tasks.front();
			_tasks.pop_front();
			this->_sem_deque_rw.release();

			auto prevState = this->_threads[index].state;
			this->_threads[index].state = ThreadHandle::State::Busy;
			// printf("start task....\n");
			call.first(call.second);
			// printf("finish task...\n");
			this->_threads[index].state = prevState;
		}

		this->_threads[index].isTerminal = true;
		// printf("see you\n");
	}

private:
	vector<ThreadHandle> _threads;
	counting_semaphore<INT32_MAX> _sem_task, _sem_deque_rw;
	atomic<bool> _end;
	deque<Call> _tasks;
};

int main()
{
	srand(time(NULL));

	ThreadPool pool(10);
	for (int i = 0; i < 15; ++i)
		pool.add_task({sleepAndPrint, nullptr});

	while (!pool.isFinish());
	printf("will terminal thread pool and kill all threads\n");
	pool.terminalAll();

}