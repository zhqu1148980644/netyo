#pragma once
#include <type_traits>

using namespace std;

template <typename Func>
using callable = std::enable_if_t<is_function<Func()>{}, int>;

class NoCopyble {
protected:
    NoCopyble() = default;
    NoCopyble(const NoCopyble &) = delete;
    NoCopyble(NoCopyble &&) = default;
    NoCopyble& operator=(const NoCopyble &) = delete;
    NoCopyble& operator=(NoCopyble &&) = default;
};
