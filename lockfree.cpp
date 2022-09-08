#include <atomic>
#include <memory>

#include <cassert>
#include <thread>
#include <algorithm>
#include <future>
#include <iomanip>
#include <vector>
#include <array>
#include <iostream>

template <typename T>
struct Channel {
//private:

    struct Node
    {
        T* data;

        std::atomic<std::int64_t> refct;
        std::atomic<Node*> next;

        template<typename ...Args>
        Node(Args&& ...args)
            : data(new T{args...}), next(nullptr), refct(1) {}

        Node()
            : data(nullptr), next(nullptr), refct(1) {}
    };

    std::atomic<Node*> tail;
    std::atomic<Node*> head;

//public:

    Channel() {
        head = new Node();
        tail = head.load();
    }

    // https://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.8674&rep=rep1&type=pdf
    template <typename ...Args>
    void push(Args&& ...args)
    {
        Node* null_node;
        Node* q = new Node{args...};

        Node* p = tail.load();
        Node* oldp = p;

        do {
            Node* null_node = nullptr;
            while (p->next != nullptr)
                p = p->next;
        } while (!p->next.compare_exchange_weak(null_node, q));

        tail.compare_exchange_strong(oldp, q);
    }

    void release(Node* n) {
        auto prev = n->refct.fetch_sub(1);
        if (prev == 1) {
            delete n->data;
            delete n;
        }
    }

    void acquire(Node* n) {
        auto prev = n->refct.fetch_add(1);
    }

    Node* pop()
    {
        Node* p = nullptr;
        Node* next = nullptr;
    
        do {
            p = head;

            if (p->next == nullptr)
                return nullptr;

            if (next != nullptr)
                release(next);

            next = p->next;
            acquire(next);
        } while (!head.compare_exchange_weak(p, next));

        release(p);

        return next;
    }

};

auto main(int argc, char* argv[]) -> int {
    static Channel<int> channel;

    const int num_writers = argc > 1 ? std::atoi(argv[1]) : 3;
    const int num_readers = argc > 2 ? std::atoi(argv[2]) : 1;
    const size_t num_tasks = num_writers + num_readers;

    using ResultArray = std::array<int, 16>;

    const auto writefunc = [](int idx) -> ResultArray {
        ResultArray res{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < (idx + 1) * 10; ++i)
            for (int j = 0; j < res.size(); ++j) {
                channel.push(j);
                res[j] += 1;
            }
        return res;
    };

    const auto readfunc = [](int idx) -> ResultArray {
        ResultArray res{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        auto value = channel.pop();
        for (; value; value = channel.pop()) {
            int x = *(value->data);
            res[x] += 1;
            channel.release(value);
        }
        return res;
    };

    std::vector<std::future<ResultArray>> tasks;
    for (int i = 0; i < num_tasks; ++i) {
        if (i == num_writers)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        tasks.push_back(std::async(std::launch::async, i < num_writers ? writefunc : readfunc, i));
    }

    int done_tasks = 0;
    while (done_tasks < num_tasks) {
        std::cout << "Waiting for tasks..." << done_tasks << '/' << num_tasks << '\r';
        using namespace std::literals;
        for (auto& task : tasks)
            if (task.wait_for(100ms) == std::future_status::ready)
                ++done_tasks;
        std::cout << std::flush;
    }

    unsigned total_written = 0;
    for (int i = 0; i < num_writers; ++i) {
        auto tmp = tasks[i].get();
        for (int j = 0; j < tmp.size(); ++j)
            total_written += tmp[j];
    }
    unsigned total_read = 0;
    for (int i = num_writers; i < num_tasks; ++i) {
        auto tmp = tasks[i].get();
        for (int j = 0; j < tmp.size(); ++j)
            total_read += tmp[j];
    }

    std::cout << "\nRead " << total_read << '/' << total_written << '\n';

    for (auto value = channel.pop(); value; value = channel.pop()) {
        ++total_read;
        channel.release(value);
    }
    std::cout << "Read " << total_read << '/' << total_written << '\n';
    
}
