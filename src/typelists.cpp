//
// Created by antonio-filipovic on 4/5/25.
//
#include <string>
#include <iostream>
#include <cassert>

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


    static_assert(std::is_same_v<Widget, TL::TypeAt<MyType, 2>::Result>);
    static_assert(std::is_same_v<std::string, TL::TypeAt<MyType, 1>::Result>);
    static_assert(std::is_same_v<int, TL::TypeAt<MyType, 0>::Result>);

    /**
     * TypeExists type exercise
     */
    constexpr bool exists_int = TL::TypeExists<MyType, int>::exists;
    std::cout << "Exists int type: " << std::boolalpha << exists_int << std::endl;

    constexpr bool exists_string = TL::TypeExists<MyType, std::string>::exists;
    std::cout << "Exists string type: " << std::boolalpha << exists_string << std::endl;

    constexpr bool exists_widget = TL::TypeExists<MyType, Widget>::exists;
    std::cout << "Exists widget type: " << std::boolalpha << exists_widget << std::endl;

    constexpr bool exists_double = TL::TypeExists<MyType, double>::exists;
    std::cout << "Exists double type: " << std::boolalpha << exists_double << std::endl;


    /**
     *
     * EraseType type exercise
     */

    static_assert(std::is_same_v<Typelist<std::string, Typelist<Widget, NullType>>, TL::EraseType<MyType, int>::Result>);
    static_assert(std::is_same_v<Typelist<int, Typelist<Widget, NullType>>, TL::EraseType<MyType, std::string>::Result>);
    static_assert(std::is_same_v<Typelist<int, Typelist<std::string, NullType>>, TL::EraseType<MyType, Widget>::Result>);


    // std::is_same_v<  Typelist<Widget, NullType>,
    //                  Typelist<int, Typelist<Typelist<Widget, NullType>, NullType>>
    //          >

    TL::GenerateClasses<TYPELIST_1(int, std::string),TL::WidgetValue> big_class{};


    //static_cast<TL::WidgetValue<std::string > &>(big_class).value_ = "a";
    //TL::Field<decltype(big_class), int>(big_class) = 2;

}
