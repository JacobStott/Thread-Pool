// Thread_Pool.cpp
// Implementation of ThreadPool class defined in Thread_Pool.h

#include "Thread_Pool.h"
#include <iostream>
#include <stack>


ThreadPool::ThreadPool(size_t numThreads) {
    if (numThreads == 0) numThreads = 1;
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] { //Each worker thread waits for tasks to be available, then takes one and executes it
            while (true) {
                TaskBasic task;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [this] { return !tasks.empty() || stop; });
                    if (stop && tasks.empty()) {//We only stop the pool once we have the stop flag and no tasks ready to execute.
                        return;                 //Tasks will continue to be added from waiting map until we've added all tasks possible to the active task pool.
                    }
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                
                task.task();//Executes outside Lock for concurrency
                task_finished(task.name);
            }
        });
    }
}


//Blocks the addition of new tasks with stop.
//Then drains out all tasks that are active or possible in the waiting map
void ThreadPool::shutdown() {
    {
    std::lock_guard<std::mutex> lock(mtx);
    stop = true;
    }
    
    cv.notify_all();

    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    //We clear the maps to ensure that anything that won't drain because of dependencies throws a broken promise instead of sitting in limbo.
    cycle_task_map.clear();
    waiting_map.clear();

}

void ThreadPool::addtask(std::string name, std::vector<std::string> dependencies, std::function<void()> task) {
    TaskDetails details{name, {}, dependencies, task};
    if(cycle_task_map.find(name) != cycle_task_map.end()) {//Duplicate Names will break everything
        throw std::runtime_error("Duplicate task name: " + name);
    }
    cycle_task_map.emplace(name, details);


    if(cycle_check(details)){
        cycle_task_map.erase(name);
        throw std::runtime_error("Cycle caused by adding task: " + name);
    }


    for(std::string dependent : dependencies) { //Checks for already finished dependencies
        auto found_dependent = finished_map.find(dependent);
        if(found_dependent != finished_map.end()) {
            cycle_task_map[name].finished_dependencies.push_back(dependent);
        }
    }

    if(cycle_task_map[name].finished_dependencies.size() == dependencies.size()) { //All dependencies are met, add to task queue
        TaskBasic newTask{name, task};
        tasks.push(newTask);
        cv.notify_one();
        return;
    } else {
        waiting_map[name] = cycle_task_map[name];
    }
}

void ThreadPool::task_finished(std::string name) {
    std::lock_guard<std::mutex> lock(mtx);
    if (finished_map.find(name) == finished_map.end()) {
        finished_map[name] = cycle_task_map[name].name;
    }

    std::vector<std::string> to_delete;
    for (auto& [id, details] : waiting_map) {//Checks waiting tasks for those dependent on the finished task, and updates them
                                             //Adds waiting tasks that are now satisified to tasks.

        for(std::string dependent : details.waiting_dependencies) {//Written assuming no task will have a high number of dependencies.
            if(dependent == name) {                                //If we have highly dependent tasks, this will move towards O(n^2) and may need rewrite
                waiting_map[id].finished_dependencies.push_back(dependent);
            }
        }


        if(details.finished_dependencies.size() == details.waiting_dependencies.size()) {
            TaskBasic newTask{id, details.task};
            tasks.push(newTask);
            to_delete.push_back(id);
            cv.notify_one();
            
        }
    }
    for(std::string id : to_delete) {
        waiting_map.erase(id);
    }
}

bool ThreadPool::cycle_check(TaskDetails details){//DFS to check for cycles, that could have been caused by adding a new task. Only checks the region of the map connected to the new task.
    std::unordered_map<std::string, int> visited;//0=Unseen, 1=Seen on Current Path, 2=Seen in past Path
    std::stack<std::pair<std::string, bool>>  to_visit;// The value for visited is Ternary, to make sure we don't detect cycles, that are actually diamonds. i.e. We only check for cycles against the current path, not every path so far.

    to_visit.push({details.name, false});
    while(!to_visit.empty()) {
        auto [current,pathEnded] = to_visit.top();
        to_visit.pop();

        if(pathEnded){
            visited[current] = 2;
            continue;
        }

        if(visited[current]==1) {
            return true;
        }
        if(visited[current]==2) {
            continue;
        }
        visited[current] = 1;
        to_visit.push({current, true});

        auto position=cycle_task_map.find(current);
        if(position != cycle_task_map.end()) {
            for(std::string dependent : position->second.waiting_dependencies) {
                if (visited[dependent] == 1) {
                    return true;
                }

                if (visited[dependent] == 0) {
                    to_visit.push({dependent,false});
                }
            }
        }
    }
    return false;
}