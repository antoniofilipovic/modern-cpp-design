//
// Created by antonio-filipovic on 3/29/25.
//

// check what does this exactly mean
#pragma once

#include <new>
#include <cassert>
#include <iostream>
#include <atomic>

namespace global {

    template <typename T>
    class HeapStoragePolicy {

        public:
            using StorageType = T*;
            using PointerType = T*;
            using ReferenceType = T*&; // like pointer to pointer
            HeapStoragePolicy(): pointee_(getDefault()) {}

            explicit HeapStoragePolicy(T* ptr): pointee_(ptr) {}

            HeapStoragePolicy(HeapStoragePolicy const &other) : pointee_(other.pointee_){}

            HeapStoragePolicy(HeapStoragePolicy &&other) = delete;

            HeapStoragePolicy& operator==(HeapStoragePolicy const &) = delete;

            HeapStoragePolicy& operator==(HeapStoragePolicy &&) = delete;

            friend void swap(HeapStoragePolicy& lhs, HeapStoragePolicy& rhs) noexcept {
                std::cout << "swap(HeapStoragePolicy)\n";
                std::swap(lhs.pointee_, rhs.pointee_);
            }

            bool operator==(const HeapStoragePolicy & other) const {
                    return pointee_ == other.pointee_;
            }

            bool operator!=(HeapStoragePolicy const &other) const {
                    return !(*this == other);
            }


            friend PointerType getImpl(const HeapStoragePolicy& sp) {
                return sp.pointee_;
            }

            // wtf const correctness
            friend T*const & getImplRef(const HeapStoragePolicy& sp) {
                return sp.pointee_;
            }

            friend ReferenceType getImplRef(HeapStoragePolicy& sp) {
                return sp.pointee_;
            }

            void destroy() {
                if(pointee_ != nullptr) {
                    delete pointee_;
                }
            }
        private:
            T *getDefault() {
                return nullptr;
            }
            T *pointee_;
    };

    class RefCountAtomicPolicy {
        public:
            RefCountAtomicPolicy() : ref_count_(new std::atomic<int>(1)) {

            }

            RefCountAtomicPolicy(RefCountAtomicPolicy &&other) noexcept: ref_count_(other.ref_count_) {
                other.ref_count_=nullptr;
            }

            RefCountAtomicPolicy(RefCountAtomicPolicy const &other): ref_count_(other.ref_count_) {
                addRef();
            }

            bool operator==(const RefCountAtomicPolicy & other) const {
                return ref_count_ == other.ref_count_;
            }

            bool operator!=(RefCountAtomicPolicy const &other) const {
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

            RefCountAtomicPolicy &operator=(RefCountAtomicPolicy &&other)  noexcept = delete;
            RefCountAtomicPolicy &operator=(RefCountAtomicPolicy const &other)  = delete;



            friend void swap(RefCountAtomicPolicy& lhs, RefCountAtomicPolicy& rhs) noexcept {
                std::cout << "swap(RefCountPolicy)\n";
                std::swap(lhs.ref_count_, rhs.ref_count_);
            }
            void addRef() {
                ++(*ref_count_);
            }

            void removeRef() {
                --(*ref_count_);
            }

            auto release() -> bool {
                removeRef();
                if(*ref_count_ == 0) {
                    delete ref_count_;
                    return true;
                }
                return false;
            }

            [[nodiscard]] auto getRefCount() const -> int {
                return *ref_count_;
            }

            // virtual destructor not needed -> not intended to be used
            // as pointer to base
        protected:
             ~RefCountAtomicPolicy() = default;

        private:
            std::atomic<int> *ref_count_{nullptr};
    };

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

            auto release() -> bool {
                removeRef();
                if(*ref_count_ == 0) {
                    delete ref_count_;
                    return true;
                }
                return false;
            }

            [[nodiscard]] auto getRefCount() const -> int {
                return *ref_count_;
            }

            // virtual destructor not needed -> not intended to be used
            // as pointer to base
        protected:
             ~RefCountPolicy() = default;

        private:
            int *ref_count_{nullptr};
    };

    template <typename T,
              template <typename> typename StoragePolicy = HeapStoragePolicy,
              typename OwnershipPolicy = RefCountPolicy
              >

    class SmartPointer : public StoragePolicy<T>, public OwnershipPolicy {

        using OP = OwnershipPolicy;
        using SP = StoragePolicy<T>;

    public:
        explicit SmartPointer(T *ptr): SP(ptr), OP() {
        }

        SmartPointer(SmartPointer &&other) noexcept:  SP(), OP(){
            std::swap(*this, other);
            std::cout << "move ctor, ref count = " << OP::getRefCount()  << std::endl;

        }

        SmartPointer(const SmartPointer &other) noexcept: SP(other), OP(other) {
            std::cout << "copy ctor, ref count = " << OP::getRefCount()  << std::endl;

        }

        SmartPointer& operator=(const SmartPointer &other){
            if(*this == other) {
                return *this;
            }

            SmartPointer temp{other};
            std::swap(*this, temp);

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

        // should this be private?
        friend void swap(SmartPointer& lhs, SmartPointer& rhs) noexcept {
            std::cout << "swap(SmartPointer)\n";
            std::swap(static_cast<OP&>(lhs), static_cast<OP&>(rhs));
            std::swap(static_cast<SP&>(lhs), static_cast<SP&>(rhs));
        }

        ~SmartPointer() {
            std::cout << "destructor" << std::endl;
            if(OP::release()) {
                std::cout << "destroy" << std::endl;
                SP::destroy();
            }

        }

        bool operator==(SmartPointer const &other) {
            return static_cast<OP const &>(other) == static_cast<OP const &>(*this) && static_cast<SP const &>(other) == static_cast<SP const &>(*this);
        }

        bool operator !=(SmartPointer const &other) {
            return !(*this == other);
        }

    };


}

