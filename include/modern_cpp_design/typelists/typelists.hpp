#pragma once


struct NullType {};



template <typename Head, typename Tail>
struct Typelist{
    using T=Head;
    using U=Tail;
};


#define TYPELIST_0(T1) Typelist<T1, NullType>
#define TYPELIST_1(T0, T1) Typelist <T0, TYPELIST_0(T1)>


namespace TL {
    // returns length of typelist
    template <typename TList>
    struct Length {};

    template <>
    struct Length<NullType> {
        enum {length = 0};
    };

    template <typename Head, typename Tail>
    struct Length<Typelist<Head, Tail>> {
        enum {length = 1 + Length<typename Typelist<Head, Tail>::U>::length };
    };

    //returns type at index

    template <typename TList, unsigned int index>
    struct TypeAt {};


    // if index == 0
    // return type head

    template <typename Head, typename Tail>
    struct TypeAt<Typelist<Head, Tail>, 0> {
        typedef Head Result;
    };

    template <typename Head, typename Tail, unsigned int index>
    struct TypeAt<Typelist<Head, Tail>, index> {
        typedef typename TL::TypeAt<Tail, index -1>::Result Result;
    };

} // end namespace TL