//
// Created by antonio-filipovic on 3/29/25.
//

// check what does this exactly mean
#pragma once

#include <new>
#include <cassert>
#include <iostream>

namespace global {

    class RefCountPolicy {
        public:
            RefCountPolicy() : ref_count_(new int(1)) {

            }

            RefCountPolicy(RefCountPolicy &&other) noexcept: ref_count_(other.ref_count_) {
                other.ref_count_=nullptr;
            }

            RefCountPolicy(RefCountPolicy const &other): ref_count_(other.ref_count_) {
                addRef();
            }

            bool operator==(const RefCountPolicy & other) const {
                return ref_count_ == other.ref_count_;
            }

            bool operator!=(RefCountPolicy const &other) const {
                return !(*this == other);
            }

            // deleted as it requires more control
            // if we would to do something like this
            // RefCountPolicy &operator=(RefCountPolicy &&other)  noexcept {
            //     int* old_ref_count = ref_count_;
            //     ref_count_ = other.ref_count_;
            //     if((*old_ref_count)--) {
            //         // how do I destroy now the derived object???
            //     }
            //     return *this;
            // }

            RefCountPolicy &operator=(RefCountPolicy &&other)  noexcept = delete;
            RefCountPolicy &operator=(RefCountPolicy const &other)  = delete;



            friend void swap(RefCountPolicy& lhs, RefCountPolicy& rhs) noexcept {
                std::cout << "swap(RefCountPolicy)\n";
                std::swap(lhs.ref_count_, rhs.ref_count_);
            }
            void addRef() {
                ++(*ref_count_);
            }

            void removeRef() {
                --(*ref_count_);
            }

            [[nodiscard]] auto getRefCount() const -> int {
                return *ref_count_;
            }

            // virtual destructor needed
            virtual ~RefCountPolicy() {
                removeRef();
                if(*ref_count_ == 0) {
                    delete ref_count_;
                }

            }

        private:
            int *ref_count_{nullptr};
    };

    template <typename T,
              typename OwnershipPolicy = RefCountPolicy>
    class SmartPointer : public OwnershipPolicy {
        using OP = OwnershipPolicy;
    public:
        explicit SmartPointer(T *ptr): OP(), pointee_(ptr) {}

        SmartPointer(SmartPointer &&other) noexcept:  OP(), pointee_(nullptr) {
            std::swap(*this, other);

            std::cout << "move ctor, ref count = " << OP::getRefCount()  << std::endl;

        }

        SmartPointer(const SmartPointer &other) noexcept: OP(other), pointee_(other.pointee_) {

            std::cout << "copy ctor, ref count = " << OP::getRefCount()  << std::endl;

        }

        SmartPointer& operator=(const SmartPointer &other){
            if(*this == other) {
                return *this;
            }
            // properly clean up after us
            // move yourself in temp object which will clean up
            {
                SmartPointer temp{nullptr};
                std::swap(*this, temp);
            }
            // we are now empty
            assert(OP::getRefCount() == 0);
            assert(pointee_ == nullptr);
            {
                SmartPointer temp{other};
                std::swap(*this, temp);
            }

            return *this;
        }

        SmartPointer& operator=(SmartPointer &&other) noexcept{
            if(*this == other) {
                return *this;
            }

            std::swap(*this, other);
            std::cout << "move assign, ref count = " << OP::getRefCount()  << std::endl;

            return *this;
        }

        friend void swap(SmartPointer& lhs, SmartPointer& rhs) noexcept {
            std::cout << "swap(SmartPointer)\n";
            std::swap(static_cast<OP&>(lhs), static_cast<OP&>(rhs));
            std::swap(lhs.pointee_, rhs.pointee_);
        }



        ~SmartPointer() {
            std::cout << "destructor" << std::endl;
            OP::removeRef();
            if(OP::getRefCount() == 0 && pointee_ != nullptr) {
                destroy();
            }

        }

        bool operator==(SmartPointer const &other) {
            return static_cast<OP const &>(other) == static_cast<OP const &>(*this) && other.pointee_ == this->pointee_;
        }

        bool operator !=(SmartPointer const &other) {
            return !(*this == other);
        }

    private:
        void destroy() {
            std::cout << "destroy, ref_count after decrement:" <<  OP::getRefCount() << std::endl;
            assert(pointee_ != nullptr);
            delete pointee_;

        }
        // pointee, we are the owners now
        T *pointee_;

    };


}

