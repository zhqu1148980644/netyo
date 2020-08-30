#pragma once
#include <bits/c++config.h>
#include <vector>
#include <type_traits>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <array>

using namespace std;

template <typename T, typename Sequence = vector<T>>
class RingBuffer {
protected:
    using size_type = ssize_t;
    using reference = typename Sequence::reference;
    using const_reference = typename Sequence::const_reference;

    Sequence vec;
    size_type lo = 0, hi = 0;

public:
    RingBuffer() : vec(7) {}
    explicit RingBuffer(size_t count, const T & value = T())
    : vec(max((size_t)7, count), value) {}
    template <typename InputIt>
    RingBuffer(InputIt first, InputIt last)
    : vec(first, last) { lo = 0; hi = vec.size(); }
    RingBuffer(const RingBuffer& other)
    : vec(other.vec), lo(other.lo), hi(other.hi) {}
    RingBuffer(RingBuffer&& other)
    : vec(other), lo(other.lo), hi(other.hi) {}
    RingBuffer& operator=(const RingBuffer & other) {
        vec = other.vec; lo = other.lo;
        return *this;
    }
    
    size_type MOD(size_type index) const {
        size_type s = vec.size();
        return index < 0 ? s - 1 : (index >= s ? index - s : index);
    }
    size_type vsize() const { return vec.size(); }
    size_type size() const {
        return lo <= hi ? hi - lo : (vec.size() - (lo - hi));
    }
    size_type capacity() const {
        return vec.size() - 1;
    }
    bool empty() const { return size() == 0; }
    bool full() const { return size() == capacity(); }
    void reserve(ssize_t len) {
        if (len + 1 <= vec.size())
            return;
        bool need_tranfer = !(lo <= hi);
        vec.resize(len + 1);
        if (need_tranfer) {
            copy(vec.begin(), vec.begin() + hi, vec.begin() + lo);
            hi = lo + hi;
        }
    }
    void expand() { reserve(2 * capacity()); }
    void ensure_space(ssize_t len) { reserve(size() + len); }

    reference operator[](ssize_t index) { return vec[MOD(lo + index)]; }
    reference & front() const { return vec[lo]; }
    reference & back() const { return vec[MOD(lo + (size() - 1))]; }
    void pop_front() { if (size()) lo = MOD(lo + 1); }
    void pop_back() { if (size()) hi = MOD(hi - 1); }

    template <typename Val>
    void push_front(Val && val) {
        if (full()) expand();
        lo = MOD(lo - 1);
        vec[lo] = forward<val>(val);
    }
    template <typename Val>
    void push_back(Val && val) {
        if (full()) expand();
        vec[hi] = forward<Val>(val);
        hi = MOD(hi + 1);
    }

protected:
    template <bool is_peek = true, bool is_write = false>
    class _iterator {
    private:
        template<bool flag, typename _T>
        using only = enable_if_t<flag, _T>;
    public:
        using iterator_category = typename std::conditional_t<
            is_peek,
            random_access_iterator_tag, forward_iterator_tag
        >;
        using difference_type = typename Sequence::difference_type;
        using reference = typename std::conditional_t<is_peek, const T &, T &>;
        using pointer = typename std::conditional_t<is_peek, const T *, T *>;
        using value_type = T;
        
        using index_type = typename
            std::conditional_t<is_peek, size_type, size_type &>;
        using dereference_type = typename
            std::conditional_t<is_write, _iterator<is_peek, is_write> &, reference>;
    private:
        RingBuffer<value_type>* buffer;
        index_type lo;
        
        size_type MOD(size_type index) {
            size_type s = buffer->vec.size();
            return index < 0 ? s - 1 : (index >= s ? index - s : index);
        }
    public:
        _iterator(RingBuffer<T>& buffer, index_type lo)
            : buffer(&buffer), lo(lo) {}
        _iterator(const _iterator<is_peek> & iter)
            : buffer(iter.buffer), lo(iter.lo) {}
        template <bool p = is_peek>
        only<p, _iterator&> operator=(const _iterator& other) {
            buffer = other.buffer; lo = other.lo;
            return *this;
        }
        
        pointer operator->() {
            return &buffer->vec[lo];
        }
        // *it
        template <bool w = is_write>
        only<w, _iterator&> operator*() {
            return *this;
        }
        template <bool w = is_write>
        only<!w, reference> operator*() {
            return buffer->vec[lo];
        }

        // it = val
        template <bool w = is_write, typename Val>
        only<w, _iterator&> operator=(Val && val) {
            if (buffer->full()) {
                buffer->expand();
            }
            buffer->vec[lo] = forward<Val>(val);
            return *this;
        }
        // ++it; it++;
        _iterator& operator++() {
            lo = MOD(lo + 1); return *this;
        }
        _iterator operator++(int) {
            _iterator tmp(*this);
            lo = MOD(lo + 1); return tmp;
        }
        
        // it + 2; it += 2;
        template<bool p = is_peek>
        only<p, _iterator&> operator--() {
            lo = MOD(lo - 1); return *this;
        }
        template<bool p = is_peek>
        only<p, _iterator> operator--(int) {
            _iterator tmp(*this);
            lo = MOD(lo - 1); return tmp;
        }
        template<bool p = is_peek>
        only<p, _iterator> operator+(difference_type diff) {
            return {buffer, MOD(lo + diff)};
        }
        template<bool p = is_peek>
        only<p, _iterator&> operator+=(difference_type diff) {
            lo = MOD(lo + diff); return *this;
        }
        bool operator==(const _iterator & other) {
            return !(!this != other);
        }
        bool operator!=(const _iterator & other) {
            return !(lo == other.lo);
        }
    };

public:
    using iterator = _iterator<true, false>;
    using const_iterator = iterator;
    using riterator = _iterator<false, false>;
    using witerator = _iterator<false, true>;
    
    iterator begin() { return {*this, lo}; }
    iterator end() { return {*this, hi}; }
    riterator read_begin() { return {*this, lo}; }
    riterator read_end() { return {*this, hi}; }
    witerator write_begin() { return {*this, hi}; }
    witerator write_end() { return {*this, lo}; }
    
    pair<iterator, iterator> operator()() const {
        return {begin(), end()};
    }
    pair<riterator, riterator> reader() {
        return {read_begin(), read_end()};
    }
    pair<witerator, witerator> writer() {
        return {write_begin(), write_end()};
    }
};

template <typename T, typename Sequence = vector<T>>
class FixedRingBuffer : public RingBuffer<T, Sequence> {
private:
    using Buffer = RingBuffer<T, Sequence>;
    using Buffer::vec, Buffer::lo, Buffer::hi;
public:
    using Buffer::Buffer, Buffer::size, Buffer::full, Buffer::MOD;

    void pop_front() {
        if (size()) {
            vec[lo] = T();
            lo = MOD(lo + 1); // try to deallocate
        }
    }
    void pop_back() {
       if (size()) {
            hi = MOD(hi - 1);
            vec[hi - 1] = T(); // try to deallocate
        }
    }

    template <typename Val>
    void push_front(Val && val) {
        if (full()) {
            hi = MOD(hi - 1);
            vec[hi] = T(); // deallocate
        }
        lo = MOD(lo - 1);
        vec[lo] = forward<val>(val);
    }
    template <typename Val>
    void push_back(Val && val) {
        if (full()) {
            vec[lo] = T(); // deallocate
            lo = MOD(lo + 1);
        }
        vec[hi] = forward<Val>(val);
        hi = MOD(hi + 1);
    }
};


class Buffer : public RingBuffer<char> {
public:
    ssize_t feed_rbuffer(int fd) {
        ssize_t bytes_feed = 0, total = 0;
        size_t len1, len2;
        do {
            if (capacity() - size() <= (capacity() >> 2)) {
                expand();
            }
            if (lo <= hi) {
                len1 = vsize() - hi; len2 = lo;
            }
            else {
                len1 = lo - hi; len2 = 0;
            }
            if (len1 > 0)  len1 -= 1;
            else           len2 -= 1; // keep the ring buffer not oversized
            iovec iovec[2] {{vec.data() + hi, len1}, {vec.data(), len2}};
            bytes_feed = ::readv(fd, iovec, len2 > 0 ? 2 : 1);
            if (bytes_feed > 0) {
                hi = MOD(hi + bytes_feed);
                total += bytes_feed;
            }
        } while (bytes_feed > 0);

        return total;
    }
    
    ssize_t drain_wbuffer(int fd) {
        if (size() <= 0) return 0;
        ssize_t remain_size = size();
        size_t len1, len2, bytes_drain = 0;
        if (lo < hi) {
            len1 = hi - lo; len2 = 0;
        }
        else {
            len1 = vsize() - lo; len2 = hi;
        }
        ssize_t wn = 0;
        if (len1) {
            wn = ::write(fd, static_cast<void*>(vec.data() + lo), len1);
            if (wn < 0) return wn; bytes_drain += wn;
        }
        if (wn == len1 && len2) {
            wn = ::write(fd, static_cast<void*>(vec.data()), len2);
            if (wn < 0) return wn; bytes_drain += wn;
        }
        lo = MOD(lo + bytes_drain);
        return remain_size - bytes_drain;
    }
    
    ssize_t feed_wbuffer(const void* data, ssize_t len) {
        if (len <= 0) return 0;
        ensure_space(len);
        if (hi <= lo) {
            memcpy(vec.data() + hi, data, len);
        }
        else {
            ssize_t len1 = min(vsize() - hi, len);
            if (len1) {
                memcpy(vec.data() + hi, data, len1);
            }
            if (len - len1 > 0) {
                memcpy(vec.data(), (void*)((char*)data + len1), len - len1);
            }
        }
        hi = MOD(hi + len);
        return len;
    }

    ssize_t drain_rbuffer(void* data, ssize_t len) {
        if (len <= 0 || !size()) return 0;
        len = min(len, size());
        if (lo <= hi) {
            memcpy(data, vec.data() + lo, len);
        }
        else {
            int len1 = vsize() - lo;
            if (len1) {
                memcpy(data, vec.data() + lo, len1);
            }
            if (len - len1 > 0) {
                memcpy((void*)((char*)data + len1), vec.data(), len - len1);
            }
        }
        return len;
    }
};
