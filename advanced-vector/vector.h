#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template<typename T>
class RawMemory {
public:
    RawMemory() = default;

    RawMemory(const RawMemory &) = delete;

    RawMemory &operator=(const RawMemory &rhs) = delete;

    RawMemory(RawMemory &&other) noexcept {
        Swap(other);
    }

    RawMemory &operator=(RawMemory &&other) noexcept {
        capacity_ = other.capacity_;
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        return *this;
    }


    explicit RawMemory(size_t capacity)
            : buffer_(Allocate(capacity)), capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T *operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T *operator+(size_t offset) const noexcept {
        return const_cast<RawMemory &>(*this) + offset;
    }

    const T &operator[](size_t index) const noexcept {
        return const_cast<RawMemory &>(*this)[index];
    }

    T &operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory &other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T *GetAddress() const noexcept {
        return buffer_;
    }

    T *GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T *Allocate(size_t n) {
        return n != 0 ? static_cast<T *>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T *buf) noexcept {
        operator delete(buf);
    }

    T *buffer_ = nullptr;
    size_t capacity_ = 0;
};

template<typename T>
class Vector {
public:
//-------------------------------конструкторы-----------------------------------------
    Vector() = default;

    explicit Vector(size_t size)
            : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector &other)
            : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector &&other) noexcept {
        Swap(other);
    }
//------------------------------------итераторы------------------------------------------
    using iterator = T *;
    using const_iterator = const T *;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
//------------------------------------------------------------------------------------------


    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) { return; }
        RawMemory<T> new_data(new_capacity);
        CopyOrMove(new_data);
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void CopyOrMove(RawMemory<T> &new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else { std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress()); }
    }


    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    Vector &operator=(const Vector &rhs) {
        if (this == &rhs) {
            return *this;
        }
        if (rhs.size_ > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
            return *this;
        }
        if (rhs.size_ < size_) {
            std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
            std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        } else {
            std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
            std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
        }
        size_ = rhs.size_;
        return *this;
    }

    Vector &operator=(Vector &&rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    const T &operator[](size_t index) const noexcept {
        return const_cast<Vector &>(*this)[index];
    }

    T &operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector &other) noexcept {
        data_.Swap(other.data_);
        size_ = std::exchange(other.size_, size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PopBack() noexcept {
        (data_.GetAddress() + size_ - 1)->~T();
        --size_;
    }

    void PushBack(const T& value) { EmplaceBack(value); }
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template<typename...Types>
    T &EmplaceBack(Types &&...values) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : data_.Capacity() * 2);
            new(new_data.GetAddress() + size_) T(std::forward<Types>(values)...);
            CopyOrMove(new_data);
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new(data_.GetAddress() + size_) T(std::forward<Types>(values)...);
        }
        return *(data_.GetAddress() + size_++);
    }

    template<typename...Types>
    iterator Emplace(const_iterator pos, Types &&...values) {
        if (pos == end()) {
            EmplaceBack(std::forward<Types>(values)...);
            return end() - 1;
        }
        int pos_index = pos - begin();
        iterator pos_non_const = begin() + pos_index;
        if (size_ < data_.Capacity()) {
            new(end()) T(std::move(*(end() - 1)));
            std::move_backward(pos_non_const, end() - 1, end());
            *pos_non_const = T(std::forward<Types>(values)...);

        } else {
            RawMemory<T> new_data(size_ == 0 ? 1 : data_.Capacity() * 2);
            new(new_data.GetAddress() + pos_index) T(std::forward<Types>(values)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), pos_index, new_data.GetAddress());
                std::uninitialized_move_n(pos_non_const, size_ - pos_index, new_data.GetAddress() + pos_index + 1);
            } else {
                std::uninitialized_copy_n(begin(), pos_index, new_data.GetAddress());
                std::uninitialized_copy_n(pos_non_const, size_ - pos_index, new_data.GetAddress() + pos_index + 1);
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return data_.GetAddress() + pos_index;
    }

    iterator Insert(const_iterator pos, const T &value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T &&value) {
        return Emplace(pos, std::move(value));

    }

    iterator Erase(const_iterator pos) {
        int pos_index = pos - begin();
        std::move(begin() + pos_index + 1, end(), begin() + pos_index);
        --size_;
        end()->~T();
        return begin() + pos_index;
    }

    ~Vector() {
        std::destroy_n(begin(), size_);
    }

private:
    static void CopyConstruct(T *buf, const T &elem) {
        new(buf) T(elem);
    }

    static void Destroy(T *buf) noexcept {
        buf->~T();
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};