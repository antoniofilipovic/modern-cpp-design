//
// Created by antonio-filipovic on 4/5/25.
//
#include <string>
#include <iostream>

#include "typelists.hpp"


class Widget {
    public:
        Widget() = default;
    private:
        int value_;
};


int main(){
    using  MyType = Typelist<int, Typelist<std::string, Typelist<Widget, NullType>>>;
    TYPELIST_0(Widget) mytype;
    TYPELIST_1(Widget, Widget) mytype2;

    std::cout << TL::Length<MyType>::length << std::endl ;


    TL::TypeAt<MyType, 3>::Result my_int;
    static_assert(std::is_same_v<int, decltype(my_int)>);
}
