#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_), capacity_(other.capacity_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            std::swap(buffer_, other.buffer_);
            std::swap(capacity_, other.capacity_);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(),other.size_,data_.GetAddress());
    }


    Vector(Vector&& other) noexcept : data_(std::move(other.data_)), size_(other.size_) {
        other.size_ = 0;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = rhs.size_;
            rhs.size_ = 0;
        }
        return *this;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if (size_ > rhs.size_) {
                    for (size_t i = rhs.size_; i < size_; ++i) {
                        std::destroy_at(data_.GetAddress() + i);
                    }
                }

                for (size_t i = 0; i < std::min(size_, rhs.size_); ++i) {
                    data_[i] = rhs.data_[i];
                }

                if (size_ < rhs.size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                              rhs.size_ - size_,
                                              data_.GetAddress() + size_);
                }

                size_ = rhs.size_;
            }
        }
        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Resize(size_t new_size) {
        if (size_ == new_size) {
            return;
        }
        if (size_ < new_size) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        } else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }


    void PushBack(const T& value) {
        EmplaceBack(value);
    }


    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }


    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            CopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return  data_[size_ - 1];
    }


    void PopBack() noexcept {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        CopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());

        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return begin();
    }
    const_iterator end() const noexcept {
        return end();
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if(size_ == Capacity()) {
            return Relocate(pos, std::forward(args)...);
        } else {
            size_t index = static_cast<size_t>(pos - begin());
            new (end()) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = T(std::forward<Args>(args)...);
            ++size_;
            return begin() + index;
        }
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        size_t index = static_cast<size_t>(pos - begin());
        std::move(begin() + index + 1, end(), begin() + index);
        data_[size_ - 1].~T();
        --size_;
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, value);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    template <typename... Args>
    iterator Relocate(const_iterator pos, Args&&... args) {
        size_t index = static_cast<size_t>(pos - begin());
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + index) T(std::forward<Args>(args)...);
        CopyOrMove(data_.GetAddress(), index, new_data.GetAddress());
        CopyOrMove(data_+index, size_ - index, new_data + (index+1));
        data_.Swap(new_data);
        ++size_;
        return begin() + index;
    }


    void CopyOrMove(T* from, size_t size, T* to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, size, to);
        } else {
            std::uninitialized_copy_n(from, size, to);
        }
        std::destroy_n(from, size);
    }

};
