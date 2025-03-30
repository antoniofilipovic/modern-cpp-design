//
// Created by antonio-filipovic on 3/29/25.
//
#include <new>
#include <iostream>
#include "smart_pointer.hpp"

int main(){
    global::SmartPointer smart_pointer{new int(2)};


    global::SmartPointer<int> smart_pointer2 = smart_pointer;

    smart_pointer2 = std::move(smart_pointer2);

    std::cout << smart_pointer2.getRefCount() << std::endl;


    return 0;
}


