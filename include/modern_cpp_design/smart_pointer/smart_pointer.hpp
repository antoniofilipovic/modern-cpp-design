//
// Created by antonio-filipovic on 3/29/25.
//

// check what does this exactly mean
#pragma once

#include <new>
#include <cassert>
#include <iostream>

namespace global {

    template <typename T>
    class SmartPointer{

    public:
        explicit SmartPointer(T *ptr): pointee_(ptr), ref_count_(new int(1)) {}

        SmartPointer(SmartPointer &&other) noexcept: pointee_(other.pointee_), ref_count_(other.ref_count_) {
            other.pointee_ = nullptr;
            other.ref_count_ = nullptr;
            std::cout << "move ctor, ref count = " << *ref_count_  << std::endl;

        }

        SmartPointer(const SmartPointer &other) noexcept: pointee_(other.pointee_), ref_count_(other.ref_count_) {
            (*ref_count_)++;
            std::cout << "copy ctor, ref count = " << *ref_count_  << std::endl;

        }

        SmartPointer& operator=(const SmartPointer &other){
            if(*this == other) {
                return *this;
            }


            destroy();

            pointee_ = other.pointee_;
            ref_count_ = other.ref_count_;
            ++(*ref_count_);

            std::cout << "copy assign, ref count = " << *ref_count_  << std::endl;

            return *this;
        }

        SmartPointer& operator=(SmartPointer &&other) noexcept{
            if(*this == other) {
                return *this;
            }
            pointee_ = other.pointee_;
            ref_count_ = other.ref_count_;

            other.pointee_ = nullptr;
            other.ref_count_ = nullptr;
            std::cout << "move assign, ref count = " << *ref_count_  << std::endl;

            return *this;
        }

        [[nodiscard]] auto getRefCount() const -> int {
            return *ref_count_;
        }

        ~SmartPointer() {
            std::cout << "destructor" << std::endl;
            destroy();
        }

        bool operator==(SmartPointer const &other) {
            return other.pointee_ == this->pointee_ && other.ref_count_ == this->ref_count_;
        }

        bool operator !=(SmartPointer const &other) {
            return !(*this == other);
        }

    private:
        void destroy() {
            if(ref_count_ != nullptr) {
                --(*ref_count_);
            }
            std::cout << "destroy, ref_count after decrement:" <<  (ref_count_ != nullptr ? *ref_count_ : -11) << std::endl;
            if(ref_count_!= nullptr && *ref_count_ == 0) {
                std::cout << "delete" << std::endl;
                assert(pointee_ != nullptr);
                delete pointee_;
            }
        }
        // pointee, we are the owners now
        T *pointee_;

        int* ref_count_;
    };


}

