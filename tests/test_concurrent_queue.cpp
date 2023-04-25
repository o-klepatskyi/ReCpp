#include <rpp/concurrent_queue.h>
#include <rpp/future.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
using namespace rpp;
using namespace std::chrono_literals;

TestImpl(test_concurrent_queue)
{
    TestInit(test_concurrent_queue)
    {
    }

    TestCase(push_and_pop)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        std::string item3 = "item3";
        queue.push(item3); // copy
        AssertThat(queue.size(), 3);
        AssertThat(queue.safe_size(), 3);
        AssertThat(queue.empty(), false);

        AssertThat(queue.pop(), "item1");
        AssertThat(queue.pop(), "item2");
        AssertThat(queue.pop(), "item3");
        AssertThat(queue.size(), 0);
        AssertThat(queue.safe_size(), 0);
        AssertThat(queue.empty(), true);

        queue.push_no_notify("item4");
        AssertThat(queue.size(), 1);
        AssertThat(queue.safe_size(), 1);
        AssertThat(queue.empty(), false);
        AssertThat(queue.pop(), "item4");
    }

    TestCase(clear)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");
        AssertThat(queue.size(), 3);
        AssertThat(queue.empty(), false);
        queue.clear();
        AssertThat(queue.size(), 0);
        AssertThat(queue.empty(), true);
    }

    TestCase(atomic_copy)
    {
        concurrent_queue<std::string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");
        std::deque<std::string> items = queue.atomic_copy();
        AssertThat(items.size(), 3);
        AssertThat(items.at(0), "item1");
        AssertThat(items.at(1), "item2");
        AssertThat(items.at(2), "item3");
    }

    // try_pop() is excellent for polling scenarios
    // if you don't want to wait for an item, but just check
    // if any work could be done, otherwise just continue
    TestCase(try_pop)
    {
        concurrent_queue<std::string> queue;
        std::string item;
        AssertThat(queue.try_pop(item), false);
        AssertThat(item, "");

        queue.push("item1");
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, "item1");
        AssertThat(queue.try_pop(item), false);
        AssertThat(item, "item1");

        queue.push("item2");
        queue.push("item3");
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, "item2");
        AssertThat(queue.try_pop(item), true);
        AssertThat(item, "item3");
        AssertThat(queue.try_pop(item), false);
        AssertThat(item, "item3");
    }

    // wait_pop() is best used for producer/consumer scenarios
    // for long-lived service threads that don't have any cancellation mechanism
    TestCase(wait_pop_producer_consumer)
    {
        concurrent_queue<std::string> queue;

        cfuture<void> producer = rpp::async_task([&] {
            queue.push("item1");
            queue.push("item2");
            queue.push("item3");
        });

        cfuture<void> consumer = rpp::async_task([&] {
            AssertThat(queue.wait_pop(), "item1");
            AssertThat(queue.wait_pop(), "item2");
            AssertThat(queue.wait_pop(), "item3");
        });

        producer.get();
        consumer.get();
    }

    // 
    TestCase(wait_pop_with_timeout)
    {
        concurrent_queue<std::string> queue;
        std::string item;
        AssertThat(queue.wait_pop(item, 5ms), false);
        AssertThat(queue.wait_pop(item, 0ms), false);

        // if someone pushes an item if we have a huge timeout, 
        // we should get it immediately
        queue.push("item1");
        rpp::Timer t;
        AssertThat(queue.wait_pop(item, 10s), true);
        AssertLess(t.elapsed_ms(), 10);
        AssertThat(queue.wait_pop(item, 15ms), false);
    }

    // introduce a slow producer thread so we can test our timeouts
    TestCase(wait_pop_with_timeout_slow_producer)
    {
        concurrent_queue<std::string> queue;
        cfuture<void> slow_producer = rpp::async_task([&] {
            std::this_thread::sleep_for(50ms);
            queue.push("item1");
            std::this_thread::sleep_for(50ms);
            queue.push("item2");
            std::this_thread::sleep_for(50ms);
            queue.push("item3");
            std::this_thread::sleep_for(100ms);
            queue.push("item3");
        });

        std::string item;
        AssertThat(queue.wait_pop(item, 5ms), false);
        AssertThat(queue.wait_pop(item, 0ms), false);
        AssertThat(queue.wait_pop(item, 5ms), false);
        AssertThat(queue.wait_pop(item, 50ms), true); // this should not timeout
        AssertThat(item, "item1");
        rpp::Timer t;
        AssertThat(queue.wait_pop(item, 75ms), true);
        AssertLess(t.elapsed_ms(), 65);
        AssertThat(item, "item2");
        AssertThat(queue.wait_pop(item, 75ms), true);
        AssertThat(item, "item3");
        // now we enter a long wait, but we should be notified by the producer
        t.start();
        AssertThat(queue.wait_pop(item, 1000ms), true);
        AssertLess(t.elapsed_ms(), 120);
    }

    // in general the pop_with_timeout is not very useful because
    // it requires some external mechanism to notify the queue
    TestCase(wait_pop_with_timeout_and_cancelcondition)
    {
        concurrent_queue<std::string> queue;
        std::atomic_bool finished = false;
        cfuture<void> slow_producer = rpp::async_task([&] {
            std::this_thread::sleep_for(50ms);
            queue.push("item1");
            std::this_thread::sleep_for(50ms);
            queue.push("item2");
            std::this_thread::sleep_for(50ms);
            queue.push("item3");
            std::this_thread::sleep_for(50ms);
            finished = true;
            queue.notify(); // notify any waiting threads
        });

        auto cancelCondition = [&] { return (bool)finished; };
        std::string item;
        AssertFalse(queue.wait_pop(item, 1ms, cancelCondition)); // this should timeout
        AssertTrue(queue.wait_pop(item, 51ms, cancelCondition));
        AssertThat(item, "item1");
        AssertTrue(queue.wait_pop(item, 51ms, cancelCondition));
        AssertThat(item, "item2");
        AssertTrue(queue.wait_pop(item, 51ms, cancelCondition));
        AssertThat(item, "item3");
        // now wait until producer exits by setting the cancellation condition
        // this should not take longer than ~55ms
        rpp::Timer t;
        AssertFalse(queue.wait_pop(item, 1000ms, cancelCondition));
        AssertLess(t.elapsed_ms(), 55);
    }

    // ensure that wait_pop_interval actually checks the cancelCondition
    // with sufficient frequency
    TestCase(wait_pop_interval)
    {
        concurrent_queue<std::string> queue;
        cfuture<void> slow_producer = rpp::async_task([&] {
            std::this_thread::sleep_for(50ms);
            queue.push("item1");
            std::this_thread::sleep_for(50ms);
            queue.push("item2");
            std::this_thread::sleep_for(50ms);
            queue.push("item3");
        });

        // wait for 100ms with 10ms intervals, but first item will arrive within ~50ms
        std::string item;
        std::atomic_int counter = 0;

        AssertTrue(queue.wait_pop_interval(item, 100ms, 10ms, [&] { return ++counter >= 10; }));
        AssertThat(item, "item1");
        AssertGreaterOrEqual(int(counter), 5);
        AssertLess(int(counter), 10);

        // wait for 20ms with 5ms intervals, it should timeout
        counter = 0;
        AssertFalse(queue.wait_pop_interval(item, 20ms, 5ms, [&] { return ++counter >= 10; }));
        AssertGreaterOrEqual(int(counter), 4);
        AssertLess(int(counter), 6);

        // wait another 30ms with 2ms intervals, and it should trigger the cancelcondition
        counter = 0;
        AssertFalse(queue.wait_pop_interval(item, 30ms, 2ms, [&] { return ++counter >= 10; }));
        AssertThat(int(counter), 10); // it should have cancelled exactly at 10 checks

        // wait until we pop the item finally
        counter = 0;
        AssertTrue(queue.wait_pop_interval(item, 100ms, 10ms, [&] { return ++counter >= 10; }));
        AssertThat(item, "item2");
        AssertLess(int(counter), 10); // we should never have reached 10 checks

        // now wait with extreme short intervals
        counter = 0;
        AssertTrue(queue.wait_pop_interval(item, 55ms, 1ms, [&] { return ++counter >= 55; }));
        AssertThat(item, "item3");
        AssertLess(int(counter), 55); // we should never have reached the limit
        AssertGreater(int(counter), 49); // but we should have reached at least 49 checks
    }
};
