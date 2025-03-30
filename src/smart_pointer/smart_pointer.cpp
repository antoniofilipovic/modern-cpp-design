//
// Created by antonio-filipovic on 3/29/25.
//

#include "smart_pointer.hpp"

namespace global {

SmartPointer::SmartPointer(int *ptr) : pointee_(ptr) {}


SmartPointer::~SmartPointer() { delete pointee_; }

}// namespace global
