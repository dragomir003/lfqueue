#include "./mpsc.hpp"

#include <iostream>
#include <array>
#include <algorithm>

auto main(int argc, char* argv[]) -> int {

    std::array<int, 16> array{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

    using IndexType = std::uint16_t;

    auto [tx, rx] = channels::mpsc::create<int>();

    std::vector<bool> dones;
    std::vector<std::thread> writers;

    const auto func = [tx = tx, &array, &dones](int idx) mutable {
        for (int i = 0; i < (idx + 1) * 10; ++i)
            for (const auto elem : array)
                tx.send(elem);
        dones[idx] = true;
    };

    const int num_writers = argc > 1 ? std::atoi(argv[1]) : 3;
    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back(func, i);
        writers.back().detach();
        dones.emplace_back(false);
    }

    std::uintmax_t total = 0;
    while (true) {
        std::optional<int> value = rx.receive();

        if (!value && std::count(begin(dones), end(dones), true) == dones.size())
            break;
        if (!value)
            continue;

        ++total;

        std::cout << "Got(" << total << ") " << *value << '@' << &value << '\r' << std::flush;
    }
    std::cout << '\n';
}
