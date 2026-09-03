// Thread_Pool.h
// Header file for ThreadPool class, which implements a thread pool with future return values, task dependencies and cycle detection.
// Dependency management is done by only adding tasks to the execution queue when all their dependencies have been marked as finished.
// The cycle detection is done by maintaining a map of all tasks and their dependencies, and doing a DFS from the new node to check for cycles when adding a new task.

#ifndef THREAD_POOL_H
#define THREAD_POOL_H   
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <future>
#include <unordered_map>


class ThreadPool {
    public:
        ThreadPool(size_t numThreads);
        void shutdown(); //Prevents new tasks and waits for current tasks to finish, before ending the thread pool and clearing broken tasks from the maps.
        ~ThreadPool() { shutdown(); }




    /// Submits a task for execution once all named dependencies have completed.
    /// @param f        Callable; return value delivered via the returned future.
    /// @param name     Unique identifier. Dependencies reference tasks by this name.
    /// @param dependencies  Names of tasks that must finish first. May reference tasks not yet submitted.
    /// @return future holding f()'s result.
    /// @throws std::runtime_error on duplicate name, on a dependency edge that would create a cycle, or if the pool is already shutting down.
    template<typename F>
    auto submit(F&& f, std::string name, std::vector<std::string> dependencies) -> std::future<decltype(f())> {
        using return_type = decltype(f());
        auto task = std::make_shared<std::packaged_task<return_type() >>(std::forward<F>(f));

        {
            std::lock_guard<std::mutex> lock(mtx);
            if(stop){
                throw std::runtime_error("ThreadPool is being shutdown. Cannot submit new tasks.");
            }

            addtask(name, dependencies, [task]() { (*task)(); });
        }
        cv.notify_one();
        return task->get_future();
    }


    private:

        struct TaskBasic {
            std::string name;
            std::function<void()> task;
        };

        struct TaskDetails {
            std::string name;
            std::vector<std::string> finished_dependencies;
            std::vector<std::string> waiting_dependencies;
            std::function<void()> task;
        };

        std::mutex mtx;
        std::condition_variable cv;
        std::queue<TaskBasic> tasks;
        bool stop = false;
        std::vector<std::thread> workers;

        std::unordered_map<std::string, TaskDetails> cycle_task_map; //Holds all tasks for cycle detection. Tasks never removed except on cycle detection. Guarded by Mtx
        std::unordered_map<std::string, TaskDetails> waiting_map; //Holds tasks waiting for dependencies. Guarded by Mtx
        std::unordered_map<std::string, std::string> finished_map; //Holds completed tasks.

        void addtask(std::string name, std::vector<std::string> dependencies, std::function<void()> task); //Must only be called with mtx locked
        void task_finished(std::string name); //Called to mark a task as finished and update waiting tasks. Mtx Must not be held by caller. It aquires it.
        bool cycle_check(TaskDetails details); //Checks cycle_task_map for cycles(deadlocks) when adding a new task. Must only be called with mtx locked

};

#endif