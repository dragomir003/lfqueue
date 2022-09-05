#pragma once

#include <utility>
#include <type_traits>
#include <optional>
#include <memory>
#include <queue>
#include <thread>
#include <chrono>
#include <list>
#include <mutex>

namespace channels::mpsc {

template<typename T>
struct Channel {
    std::queue<T, std::list<T>> queue{};
    std::mutex qmutex;

    void enqueue(T&& value) {
        std::lock_guard<std::mutex> lg(qmutex);
        queue.push(std::move(value));
    }
    
    std::optional<T> dequeue() {
        std::optional<T> val = std::nullopt;
        {
            std::lock_guard lg(qmutex);
            if (queue.size() > 0) {
                val = std::move(queue.front());
                queue.pop();
            }
        }

        return val;
    }

};

template <typename T>
struct Producer;

template <typename T>
struct Consumer;

template <typename T>
auto create() -> std::pair<Producer<T>, Consumer<T>>;

template <typename T>
struct Producer {
    Producer() = default;

    Producer(const Producer<T>& other) = default;
    Producer(Producer<T>&& other) = default;

    Producer<T>& operator=(const Producer<T>& other) = default;
    Producer<T>& operator=(Producer<T>&& other) = default;

    auto send(T value) -> void {
        if (is_valid())
            channel->enqueue(std::move(value));
    }
    
    auto is_valid() -> bool {
        return (bool)channel;
    }

    friend auto create<T>() -> std::pair<Producer<T>, Consumer<T>>;
private:
    std::shared_ptr<Channel<T>> channel;
};

template <typename T>
struct Consumer {
    Consumer() = default;

    Consumer(const Consumer<T>& other) = delete;
    Consumer(Consumer<T>&& other) = default;

    Consumer<T>& operator=(const Consumer<T>& other) = delete;
    Consumer<T>& operator=(Consumer<T>&& other) = default;

    auto receive() -> std::optional<T> {
        return is_valid() ? channel->dequeue() : std::nullopt;
    }

    auto do_producers_exist() -> bool {
        return channel.use_count() > 1;
    }

    auto is_valid() -> bool {
        return (bool)channel;
    }

    friend auto create<T>() -> std::pair<Producer<T>, Consumer<T>>;
private:
    std::shared_ptr<Channel<T>> channel;
};

template <typename T>
auto create() -> std::pair<Producer<T>, Consumer<T>> {
    Producer<T> tx;
    Consumer<T> rx;

    auto channel = std::make_shared<Channel<T>>();

    tx.channel = channel;
    rx.channel = channel;
    
    return std::make_pair(tx, std::move(rx));
}

}
