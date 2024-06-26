#include <rpp/tests.h>
#include <rpp/thread_pool.h>
#include <rpp/semaphore.h>
#include <rpp/timer.h> // performance measurement
#include <atomic>
#include <unordered_set>
using namespace rpp;
using namespace std::this_thread;
using namespace std::chrono_literals;

namespace rpp
{
    inline rpp::string_buffer& operator<<(rpp::string_buffer& sb, rpp::wait_result wr)
    {
        sb << (wr == wait_result::finished ? "finished" : "timeout");
        return sb;
    }
}


TestImpl(test_threadpool)
{
    TestInit(test_threadpool)
    {
        print_info("global_max_parallelism: %d\n", thread_pool::global_max_parallelism());
    }

    TestCleanup()
    {
        thread_pool::global().clear_idle_tasks();
        if (thread_pool::global().active_tasks() > 0)
            print_error("Dangling tasks detected: %d\n", thread_pool::global().active_tasks());
    }

    static int parallelism_count(int numIterations)
    {
        std::mutex m;
        std::unordered_set<std::thread::id> ids;
        parallel_for(0, numIterations, 0, [&](int start, int end)
        {
            (void)start; (void)end;
            ::sleep_for(1ms);
            { std::lock_guard<std::mutex> lock{m};
                ids.insert(::get_id());
            }
        });
        std::lock_guard<std::mutex> lock{m};
        return (int)ids.size();
    }

    TestCase(threadpool_concurrency)
    {
        AssertThat(parallelism_count(1), 1);
        AssertThat(parallelism_count(128), thread_pool::global_max_parallelism());
    }

    TestCase(generic_task)
    {
        semaphore sync;
        std::string s = "Data";
        rpp::task_delegate<void()> fun = [x=s, &s, &sync]()
        {
            //printf("generic_task: %s\n", x.c_str());
            s = "completed";
            sync.notify();
        };
        parallel_task([f=std::move(fun)]()
        {
            f();
        });

        sync.wait();
        AssertThat(s, "completed");
    }

    TestCase(parallel_for_max_range_size)
    {
        auto numbers = std::vector<int>(32);
        int* ptr = numbers.data();
        int  len = (int)numbers.size();
        for (int i = 0; i < len; ++i)
            ptr[i] = i;

        rpp::Timer t;

        thread_pool pool{4};
        pool.parallel_for(0, len, 8, [&](int start, int end) {
            AssertThat(end-start, 8);
            for (int i = start; i < end; ++i) AssertThat(ptr[i], i);
        });
        
        pool.parallel_for(0, len, 2, [&](int start, int end) {
            AssertThat(end-start, 2);
            for (int i = start; i < end; ++i) AssertThat(ptr[i], i);
        });

        pool.parallel_for(0, len, 1, [&](int start, int end) {
            AssertThat(end-start, 1);
            for (int i = start; i < end; ++i) AssertThat(ptr[i], i);
        });

        double elapsed = t.elapsed();
        AssertLessOrEqual(elapsed, 0.2);
    }

    TestCase(parallel_for_max_range_size_unaligned)
    {
        auto numbers = std::vector<int>(17);
        int* ptr = numbers.data();
        int  len = (int)numbers.size();
        for (int i = 0; i < len; ++i)
            ptr[i] = i;
        
        thread_pool pool{4};
        rpp::Timer t;

        // [0,8); [8,16); [16,17)
        pool.parallel_for(0, len, 8, [&](int start, int end) {
            if      (start == 0)  { AssertThat(end-start, 8); AssertThat(end, 8); }
            else if (start == 8)  { AssertThat(end-start, 8); AssertThat(end, 16); }
            else if (start == 16) { AssertThat(end-start, 1); AssertThat(end, 17); }
            else AssertMsg(false, "invalid parallel_for range: [%d, %d)", start, end);
        });

        double elapsed = t.elapsed();
        AssertLessOrEqual(elapsed, 0.05);

        AssertEqual(pool.idle_tasks(), 3);
    }

    TestCase(parallel_for_performance)
    {
        auto numbers = std::vector<int>(81'234'567);
        int* ptr = numbers.data();
        volatile int len = (int)numbers.size();

        parallel_for(0, len, 0, [&](int start, int end) {
            for (int i = start; i < end; ++i)
                ptr[i] = i;
        });
        for (int i = 0; i < len; ++i) {
            AssertThat(ptr[i], i);
        }
        //#pragma loop(hint_parallel(0))
        //for (int i = 0; i < len; ++i)
        //    ptr[i] = rand() % 32768;
        //concurrency::parallel_for(0, len, [&](int index) {
        //    ptr[index] = index;
        //});

        Timer timer1;

        // Continuous Integration machines are virtualized,
        // so the parallelism are shared between VM's which can lead to invalid test results
        // Attempt to detect this and limit the number of tasks
        if (thread_pool::global_max_parallelism() > 8)
        {
            print_info("Limiting Max Parallelism to 8\n");
            thread_pool::set_global_max_parallelism(8);
        }

        std::atomic<int64_t> sum { 0 };
        parallel_for(0, len, 0, [&](int start, int end) {
            int64_t isum = 0;
            for (int i = start; i < end; ++i)
                isum += ptr[i];
            sum += isum; // only touch atomic int once
        });
        //#pragma loop(hint_parallel(0))
        //for (int i = 0; i < len; ++i)
        //    sum += ptr[i];
        //concurrency::parallel_for(size_t(0), numbers.size(), [&](int index) {
        //    sum += ptr[index];
        //});
        double parallel_elapsed = timer1.elapsed();
        print_info("ParallelFor  elapsed: %.4fs  result: %lld\n", parallel_elapsed, (long long)sum);
        AssertThat((long long)sum, 3299527397221461LL);

        Timer timer2;
        int64_t sum2 = 0;
        for (int i = 0; i < len; ++i)
            sum2 += ptr[i];
        double serial_elapsed = timer2.elapsed();

        print_info("Singlethread elapsed: %.4fs  result: %lld\n", serial_elapsed, (long long)sum2);
        AssertThat((long long)sum2, 3299527397221461LL);

        int parallelism = thread_pool::global_max_parallelism();
        print_info("Test System # Max Parallelism: %d\n", parallelism);
        if (parallelism == 1)
        {
            // System has no parallelism at all, so there is going to be significant overhead!
            AssertLessOrEqual(parallel_elapsed, serial_elapsed+0.06);
        }
        else if (parallelism <= 2)
        {
            // if the system doesn't have enough parallelism, the overhead should be minimal
            AssertLessOrEqual(parallel_elapsed, serial_elapsed+0.005);
        }
        else
        {
            AssertLessOrEqual(parallel_elapsed, serial_elapsed+0.001);
        }

        print_info("Global Thread Pool idle tasks: %d\n", thread_pool::global().idle_tasks());
        print_info("Global Thread Pool active tasks: %d\n", thread_pool::global().active_tasks());
    }

    TestCase(parallel_foreach)
    {
        auto numbers = std::vector<int>(1337);
        parallel_foreach(numbers, [](int& n) {
            n = 1;
        });

        int checksum = 0;
        for (int i : numbers) checksum += i;
        AssertThat(checksum, 1337);
    }

    TestCase(repeat_tests)
    {
        for (int i = 0; i < 2; ++i)
        {
            test_threadpool_concurrency();
            test_generic_task();
            test_parallel_for_performance();
        }
    }

    TestCaseExpectedEx(parallel_task_exception, std::logic_error)
    {
        int times_launched = 0; // this makes sure the threadpool loop doesn't retrigger our task
        auto task = rpp::parallel_task([&]() {
            AssertThat(times_launched, 0);
            ++times_launched;
            throw std::logic_error("aaargh!");
        });
        task.wait(); // @note this should rethrow
    }

    TestCase(parallel_task_reentrance)
    {
        int times_launched = 0;
        auto task = rpp::parallel_task([&] {
            ++times_launched;
            ::sleep_for(10ms);
        });
        task.wait();
        AssertThat(times_launched, 1);

        task = rpp::parallel_task([&] {
            ++times_launched;
            ::sleep_for(10ms);
        });
        task.wait();
        AssertThat(times_launched, 2);
    }

    TestCase(parallel_task_resurrection)
    {
        thread_pool::global().max_task_idle_time(0.3f);
        thread_pool::global().clear_idle_tasks();
        AssertThat(thread_pool::global().active_tasks(), 0);

        std::atomic_int times_launched {0};
        rpp::parallel_task([&] {
            times_launched += 1; 
        }).wait(std::chrono::milliseconds{1000});
        AssertThat((int)times_launched, 1);

        print_info("Waiting for pool tasks to die naturally...\n");
        ::sleep_for(0.5s);

        print_info("Attempting pool task resurrection\n");
        rpp::parallel_task([&] { 
            times_launched += 1; 
        }).wait(std::chrono::milliseconds{1000});
        AssertThat((int)times_launched, 2);

        thread_pool::global().max_task_idle_time(2);
    }

    TestCase(parallel_task_nested_nodeadlocks)
    {
        std::atomic_int times_launched {0};

        auto func = [&]() {
            times_launched += 1;
            std::vector<pool_task_handle> subtasks {
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
                parallel_task([&]() { times_launched += 1; }),
            };

            for (auto& task : subtasks)
                AssertThat(task.wait(std::chrono::milliseconds{1000}), wait_result::finished);
        };
        std::vector<pool_task_handle> main_tasks = {
            parallel_task(func),
            parallel_task(func),
            parallel_task(func),
            parallel_task(func),
        };

        // TODO: this test is failing, run some stress tests
        for (auto& task : main_tasks)
            AssertThat(task.wait(std::chrono::milliseconds{2000}), wait_result::finished);

        int expected = 4 * 6;
        AssertThat((int)times_launched, expected);
    }

};
