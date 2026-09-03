# Thread Pool
A C++ Thread Pool that uses a dependency graph for scheduling. Tasks are submitted with the names of their dependencies. Tasks enter the run queue once all of their dependencies have been completed. Tasks can be submitted in any order, and upon submission it checks for cycles. Cycle detection uses iterative DFS to reject any submissions that would create a cycle. The Thread pool supports typed return values through `std::future<T>`. Tasks with duplicate names are rejected, since names are the task indexes. 

Tasks are submitted as a function that takes no arguments, a name and the names of dependencies. Tasks must have unique names, not cause cycles and the pool must not be in its shutdown phase. Tasks with unsatisfiable dependencies are dropped at shutdown. We assume that tasks have reasonably few dependencies or we'll experience slow downs from updating the dependencies due to the nested for loop.

## Dependency and Cycle Checking

It uses three maps guarded by a single mutex.

- `cycle_task_map` — All Tasks submitted
- `waiting_map` — tasks whose dependencies are not yet fully satisfied.
- `finished_map` — completed task names.

Submitted tasks are entered into the cycle task map. We comparre their dependencies, if any and then place them in the waiting map or finished map. When a task completes we add it to the finished map and update the waiting map to see if any tasks are ready to be moved into the active queue.

We create a graph of dependencies, which we add to with each submission. After each submission we run a DFS from the inserted task in the graph. Any cycle created by it, must include it. Nodes can have three states while we search, unexplored, currently exploring or has been explored. If we newly encounter a currently exploring node, that indicates a cycle, and the submission is rejected.


## Testing
`driver.cpp` exercises five scenarios:

1. **Diamond dependency** — `A → C`, `B → C`, `C → D`, submitted in reverse order. Verifies both execution ordering (via timestamps) and returned values.
2. **Chain** — 20 tasks in a linear dependency chain.
3. **Duplicate names** — confirms the expected throw.
4. **Cycle detection** — a 2-node cycle and a 50-node cycle, both expected to throw.
5. **Scaling benchmark** — single-threaded baseline versus full hardware concurrency.


