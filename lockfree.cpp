#include <atomic>
#include <memory>

#include <thread>
#include <algorithm>
#include <vector>
#include <array>
#include <iostream>

template <typename T>
struct Channel {
//private:

    struct Node
    {
        std::shared_ptr<T> data;
        std::atomic<std::shared_ptr<Node>> next;
        template<typename ...Args>
        Node(Args&& ...args):
            data(std::make_unique<T>(args...)), next(nullptr) {}
        Node()
            : data(nullptr), next(nullptr) {}
    };
    std::atomic<std::shared_ptr<Node>> tail;
    std::atomic<std::shared_ptr<Node>> head;

//public:

    using value_type = T;
    using pointer_type = std::shared_ptr<T>;

    Channel() {
        head = std::shared_ptr<Node>(new Node());
        tail = head.load();
    }

    // https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.8674&rep=rep1&type=pdf
    template <typename ...Args>
    void push(Args ...args)
    {
        std::shared_ptr<Node> null_node = nullptr;
        std::shared_ptr<Node> q = std::make_unique<Node>(args...);

        std::shared_ptr<Node> p;
        bool succ;

        do {
            p = tail.load();
            if (!(succ = p->next.compare_exchange_weak(null_node, q)))
                tail.compare_exchange_weak(p, p->next);
        } while (!succ);
        tail.compare_exchange_weak(p, q);
    }

    std::shared_ptr<T> pop()
    {
        std::shared_ptr<Node> p;
        do {
            p = head.load();
            if (p == nullptr)
                return nullptr;
        } while (!head.compare_exchange_weak(p, p->next));

        return p->data;
    }

};

auto main(int argc, char* argv[]) -> int {

    std::array<int, 16> array{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

    using IndexType = std::uint16_t;
    Channel<int> channel;

    std::vector<bool> dones;
    std::vector<std::thread> writers;

    const auto func = [&channel, &array, &dones](int idx) {
        for (int i = 0; i < (idx + 1) * 10; ++i)
            for (const auto elem : array)
                channel.push(elem);
        dones[idx] = true;
    };

    const int num_writers = argc > 1 ? std::atoi(argv[1]) : 3;
    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back(func, i);
        dones.emplace_back(false);
    }

    for (auto& writer : writers)
        writer.join();

    std::shared_ptr<int> tmp = (channel.pop(), channel.pop());
    int total = 0;
    while (tmp) {
        std::cout << *tmp << '\n';
        ++total;
        tmp = channel.pop();
    }
    std::cout << total << '\n';

    return 0;
/*
    const auto is_filled = [&dones] {
        return std::count(begin(dones), end(dones), true) == dones.size();
    };

    std::uintmax_t total = 0;
    while (true) {
        auto value = channel.pop();

        if (!value && is_filled())
            break;
        if (!value)
            continue;

        ++total;

        std::cout << "Got(" << total << ") " << *value << '@' << value << '\r' << std::flush;
    }
    std::cout << '\n';
    */
}
