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
        std::atomic<Node*> next;
        template<typename ...Args>
        Node(Args&& ...args):
            data(new T{args...}), next(nullptr) {}
        Node()
            : data(nullptr), next(nullptr) {}
    };
    std::atomic<Node*> tail;
    std::atomic<Node*> head;

//public:

    using value_type = T;
    using pointer_type = std::shared_ptr<T>;

    Channel() {
        head = new Node();
        tail = head.load();
    }

    // https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.8674&rep=rep1&type=pdf
    template <typename ...Args>
    void push(Args&& ...args)
    {
        Node* null_node = nullptr;
        Node* q = new Node{args...};

        Node* p = tail.load();
        Node* oldp = p;

        do {
            while (p->next != nullptr)
                p = p->next;
        } while (!p->next.compare_exchange_weak(null_node, q));

        tail.compare_exchange_strong(oldp, q);
    }

    std::shared_ptr<T> pop()
    {
        Node* p;
        do {
            p = head.load();
            if (p->next == nullptr)
                return nullptr;
        } while (!head.compare_exchange_strong(p, p->next));

        Node* q = p->next;
        //delete p;

        return q->data;
    }

};

auto main(int argc, char* argv[]) -> int {

    static std::array<int, 16> array{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    static Channel<int> channel;

    const int num_writers = argc > 1 ? std::atoi(argv[1]) : 3;
    const int num_readers = argc > 2 ? std::atoi(argv[2]) : 1;
    const size_t total_messages = 80 * num_writers * (num_writers + 1);

    const auto func = [](int idx) {
        for (int i = 0; i < (idx + 1) * 10; ++i)
            for (const auto elem : array)
                channel.push(elem);
    };

    std::atomic<std::uintmax_t> total = 0;
    const auto readfunc = [&total, total_messages] {
        while (total < total_messages) {
            if (auto value = channel.pop(); value != nullptr)
                ++total;
        }
    };

    std::vector<std::thread> writers;
    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back(func, i);
        writers.back().detach();
    }
    std::cout << "Init writers\n";

    std::vector<std::thread> readers;
    for (int i = 0; i < num_readers; ++i) {
        readers.emplace_back(readfunc);
        readers.back().detach();
    }
    std::cout << "Init readers\n";

    std::uintmax_t old_total = total;
    while (old_total < total_messages) {
        if (total == old_total)
            continue;
        std::cout << old_total << '\n' << std::flush;
        old_total = total;
    }
    std::cout << old_total << "\nDone\n";
}
