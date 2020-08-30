#pragma once

#include <type_traits>
#include <string_view>
#include <string>
#include <algorithm>
#include <string.h>

using namespace std;

static const char digits[] = "0123456789ABCDEF";

template <typename Ostream, size_t FSIZE = 4000>
class LogStream {
protected:
    template <bool flag, typename T_>
    using only = enable_if_t<flag, T_>;
    using self = LogStream;

    Ostream ostream;
    static char double_buffer[100], buffer[FSIZE];
    size_t cur = 0;    

public:
    self& append(char * buffer, size_t len) {
        if (sizeof(buffer) - cur < len) {
            memcpy(buffer + cur, buffer, len);
            *buffer++ = ' ';
            cur += len + 1;
        }
        return *this;
    }

    template<size_t N>
    self& operator<<(const char s[N]) {
        return append(s, N - 1);
    }

    self& operator<<(const std::string & sv) {
        return append(sv.data(), sv.size());
    }

    template <typename T>
    only<is_integral<T>::value, self&> operator<<(T val) {
        char * begin, * w;
        begin = w = buffer + cur;
        do {
            int lsd = static_cast<int>(val % 10);
            *w++ = digits[lsd]; val /= 10;
        } while (val > 0);
        if (val < 0) {
            *w++ = '-';
        }
        *w++ = ' ';
        reverse(begin, w);
        cur += w - begin;

        return *this;
    }

    self& operator<<(const long double & val) {
        int len = snprintf(double_buffer, 15, "%.12Lg", val);
        if (sizeof(buffer) - cur < len) {
            append(double_buffer, len);
        }
        return *this;
    }

    template <typename T>
    only<is_floating_point<T>::value, self&> operator<<(T val) {
        return *this << static_cast<long double>(val);
    }

    template <typename T, size_t N, bool is_char = is_same<T, char>::value>
    only<!is_char, self&> operator<<(const T (&array)[N]) {
        for (int i = 0; i  < N; ++i)
            *this << array[i];

        return *this;
    }

};

#define LOG ()