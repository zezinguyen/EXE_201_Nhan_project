#pragma once
#include <stddef.h>

// Bộ đệm vòng cố định N phần tử, đầy thì ghi đè phần tử cũ nhất.
// Dùng cho hàng đợi log lỗi gửi bù (spec: tối đa 50 bản ghi).
template <typename T, size_t N>
class RingBuffer {
    static_assert(N > 0, "RingBuffer cần N > 0");

public:
    // Trả về false nếu đã phải ghi đè phần tử cũ nhất.
    bool push(const T& item) {
        const bool overwrote = full();
        items_[(head_ + size_) % N] = item;
        if (overwrote) {
            head_ = (head_ + 1) % N;
        } else {
            size_++;
        }
        return !overwrote;
    }

    bool pop(T& out) {
        if (empty()) {
            return false;
        }
        out = items_[head_];
        head_ = (head_ + 1) % N;
        size_--;
        return true;
    }

    bool peek(T& out) const {
        if (empty()) {
            return false;
        }
        out = items_[head_];
        return true;
    }

    // Phần tử thứ index tính từ cũ nhất (0 = cũ nhất).
    const T& at(size_t index) const { return items_[(head_ + index) % N]; }

    void clear() {
        head_ = 0;
        size_ = 0;
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == N; }
    static constexpr size_t capacity() { return N; }

private:
    T items_[N] = {};
    size_t head_ = 0;
    size_t size_ = 0;
};
