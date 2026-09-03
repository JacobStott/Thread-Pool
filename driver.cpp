// driver.cpp
// Driver program to test the ThreadPool implementation
// Test 1 creates a set of tasks with dependencies and checks if they execute in the correct order, and if they return properly
// Test 2 creates a chain of 20 tasks, where each task depends on the previous one, and checks if they execute in the correct order, and if they return properly
// Test 3 checks if submitting a task with a duplicate name throws the expected error
// Test 4 checks if the cycle detection works by trying to submit tasks that would cause a cycle, and checking if the expected error is thrown
// Test 5 is a benchmark for testing how efficiency scales with number of threads



#include "Thread_Pool.h"
#include <functional>
#include <iostream>
#include <future>
#include <cmath>
#include <string>

int main() {

    ThreadPool pool(std::thread::hardware_concurrency());
    std::vector<std::future<std::string>> futureresults;

    std::vector<std::chrono::steady_clock::time_point> task_times(4);

    //Dependency Test
    //A -> C, B -> C, C -> D
    futureresults.emplace_back(pool.submit([&task_times]() -> std::string {
        std::cout << "Task D (after C)\n";
        task_times[3] = std::chrono::steady_clock::now();
        return "D";
    }, "D", {"C"}));
    futureresults.emplace_back(pool.submit([&task_times]() -> std::string {
        std::cout << "Task C (after A and B)\n";
        task_times[2] = std::chrono::steady_clock::now();
        return "C";
    }, "C", {"A", "B"}));
    futureresults.emplace_back(pool.submit([&task_times]() -> std::string {
        std::cout << "Task B\n";
        task_times[1] = std::chrono::steady_clock::now();
        return "B";
    }, "B", {}));
    futureresults.emplace_back(pool.submit([&task_times]() -> std::string {
        std::cout << "Task A\n";
        task_times[0] = std::chrono::steady_clock::now();
        return "A";
    }, "A", {}));

    pool.shutdown();
    bool dependency_test_passed = true;
    if (task_times[3]<task_times[2])
    {
        std::cout << "Task D ran before C\n";
        dependency_test_passed = false;
    }
    if (task_times[2]<task_times[0] || task_times[2]<task_times[1])
    {
        std::cout << "Task C ran before A or B\n";
        dependency_test_passed = false;
    }
    std::vector<std::string> expected_order = {"D", "C", "B", "A"};
    for(int i = 0; i < 4; i++) {
        std::string result = futureresults[i].get();
        std::string expected = expected_order[i];
        if(result != expected) {
            std::cout << "Expected " << expected << " but got " << result << "\n";
            dependency_test_passed = false;
        } else {
            std::cout << "Got expected result: " << result << "\n";
        }
    }

    if(dependency_test_passed){
        std::cout << "Dependency test Passed\n" << std::endl;
    } else {
        std::cout << "Dependency test Failed\n" << std::endl;
    }
    

    //Chain Test
    //1 -> 2 -> 3 -> ... -> 20

    ThreadPool pool_2(std::thread::hardware_concurrency());

    std::vector<std::future<int>> chain_results;
    std::vector<int> chain_expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
    std::vector<std::chrono::steady_clock::time_point> chain_times(20);

    chain_results.emplace_back(pool_2.submit([&chain_times]() -> int {
        std::cout << "Task 0\n";
        chain_times[0] = std::chrono::steady_clock::now();
        return 0;
    }, "T0", {}));

    for(int i = 1; i < 20; i++) {
        std::string name = "T" + std::to_string(i);
        std::string dependency = "T" + std::to_string(i-1);
        chain_results.emplace_back(pool_2.submit([&, i]() {
            std::cout << "Task " << (i) << "\n";
            chain_times[i] = std::chrono::steady_clock::now();
            return i;
        }, name, {dependency}));
    }

    pool_2.shutdown();

    bool chain_test_passed = true;
    for(int i = 0; i < 20; i++) {
        if(i > 0 && chain_times[i] < chain_times[i-1]) {
            std::cout << "Task " << (i+1) << " ran before Task " << i << "\n";
            chain_test_passed = false;
        }
        int result = chain_results[i].get();
        int expected = chain_expected[i];
        if(result != expected) {
            std::cout << "Expected " << expected << " but got " << result << "\n";
            chain_test_passed = false;
        } else {
            std::cout << "Got expected result: " << result << "\n";
        }
    }

    if(chain_test_passed){
        std::cout << "Chain test Passed\n" << std::endl;
    } else {
        std::cout << "Chain test Failed\n" << std::endl;
    }

    //Duplicate Name Test

    ThreadPool pool_3(std::thread::hardware_concurrency());
    bool duplicate_test_passed = false;
    try {
        pool_3.submit([] {}, "A", {});
        pool_3.submit([] {}, "A", {});
        std::cout << "Duplicate name test Failed\n" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "Caught expected error: " << e.what() << "\n";
        std::cout << "Duplicate name test Passed\n" << std::endl;
        duplicate_test_passed = true;
    }
    pool_3.shutdown();

    //Cycle Detection Test
    //A -> B -> A

    ThreadPool pool_4_1(std::thread::hardware_concurrency());
    bool cycle_test_part_1_passed = false;
    try{
        pool_4_1.submit([] {}, "A", {"B"});
        pool_4_1.submit([] {}, "B", {"A"});
    } catch (const std::runtime_error& e) {
        std::cout << "Caught expected error: " << e.what() << "\n";
        std::cout << "Cycle detection test Passed\n" << std::endl;
        cycle_test_part_1_passed = true;
    }
    pool_4_1.shutdown();

    //1->2->...->50->1
    ThreadPool pool_4_2(std::thread::hardware_concurrency());
    bool cycle_test_part_2_passed = false;

     for(int i = 1; i <= 49; i++) {
        std::string name = "C" + std::to_string(i);
        std::string dependency = "C" + std::to_string(i+1);
        pool_4_2.submit([] {}, name, {dependency});
    }
    try{
        pool_4_2.submit([] {}, "C50", {"C1"});
    } catch (const std::runtime_error& e) {
        std::cout << "Caught expected error: " << e.what() << "\n";
        std::cout << "Cycle detection test Passed\n" << std::endl;
        cycle_test_part_2_passed = true;
    }
    pool_4_2.shutdown();

    bool cycle_test_passed = cycle_test_part_1_passed && cycle_test_part_2_passed;

    if(cycle_test_passed) {
        std::cout << "Overall Cycle Detection Test Passed\n" << std::endl;
    } else {
        std::cout << "Overall Cycle Detection Test Failed\n" << std::endl;
    }

    bool passFail = dependency_test_passed && chain_test_passed && duplicate_test_passed && cycle_test_passed;

    if(passFail) {
        std::cout << "All tests passed successfully!\n" << std::endl;
    } else {
        std::cout << "Some tests failed. Please check the output for details.\n" << std::endl;
    }



    // Scaling benchmark
    std::cout << "=== Scaling Benchmark ===" << std::endl;
    int bench_tasks = 500;
    double baseline_ms = 0;
    int concurrency = std::thread::hardware_concurrency();

    for (size_t threads : {1, concurrency}) {
        ThreadPool bench_pool(threads);

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::future<double>> results;

        for (int i = 0; i < bench_tasks; i++) {
            results.emplace_back(bench_pool.submit([i]() -> double {
                volatile double sum = 0;
                for (int j = 0; j < 100000; j++) {
                    sum += std::sin(i * j * 0.001) * std::cos(j * 0.001);
                }
                return sum;
            }, "B" + std::to_string(i), {}));
        }

        for (auto& f : results) f.get();
        bench_pool.shutdown();

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        double tasks_per_sec = (bench_tasks * 1000.0) / ms;

        if (threads == 1) {
            baseline_ms = ms;
            std::cout << threads << " thread:  " << ms << " ms  |  "
                    << tasks_per_sec << " tasks/sec  |  baseline" << std::endl;
        } else {
            double speedup = baseline_ms / ms;
            double efficiency = (speedup / threads) * 100.0;
            std::cout << threads << " threads: " << ms << " ms  |  "
                    << tasks_per_sec << " tasks/sec  |  "
                    << speedup << "x speedup  |  " << efficiency << "% efficiency" << std::endl;
        }
    }

    if(passFail){
        return 0;
    }else
        return 1;

}

