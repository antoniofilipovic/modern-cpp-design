#pragma once


struct NullType {};



template <typename Head, typename Tail>
struct Typelist{
    using T=Head;
    using U=Tail;
};


#define TYPELIST_0(T1) Typelist<T1, NullType>
#define TYPELIST_1(T0, T1) Typelist <T0, TYPELIST_0(T1)>


#define TYPELIST_WITHOUT_TYPELIST_WRAPPER(T0, T1) Typelist <T0, T1>

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

    template <typename TList, typename T >
    struct TypeExists {
        enum {exists = false};
    };

    template <typename Head, typename T >
    struct TypeExists<Typelist<Head, NullType>, T> {
        enum {exists = std::is_same_v<Head, T>};
    };

    template <typename Head, typename Tail, typename T>
    struct TypeExists<Typelist<Head, Tail>, T> {
        enum {exists = std::is_same_v<Head, T> || TypeExists<Tail, T>::exists};
    };


    template <typename TList, typename T>
    struct EraseType {};


    template <typename Head, typename T>
    struct EraseType<Typelist<Head, NullType>, T>{
        using Result = Typelist<Head, NullType>;
    };

    template <typename Head >
    struct EraseType<Typelist<Head, NullType>, Head>{
       using Result = NullType;
    };

    template <typename Head, typename Tail, typename T >
    struct EraseType<Typelist<Head, Tail>, T> {
        using TailResult = typename  EraseType<Tail, T>::Result;
        using Result = Typelist<Head, TailResult>;
    };

    template <typename Head, typename Tail>
    struct EraseType<Typelist<Head, Tail>, Head> {
        using Result = Tail;
    };


    template <typename T>
    struct WidgetValue {
        T value_;
    };

    namespace Private
    {
        // The following type helps to overcome subtle flaw in the original
        // implementation of GenScatterHierarchy.
        // The flaw is revealed when the input type list of GenScatterHierarchy
        // contains more then one element of the same type (e.g. LOKI_TYPELIST_2(int, int)).
        // In this case GenScatterHierarchy will contain multiple bases of the same
        // type and some of them will not be reachable (per 10.3).
        // For example before the fix the first element of Tuple<LOKI_TYPELIST_2(int, int)>
        // is not reachable in any way!
        template<class, class>
        struct ScatterHierarchyTag;
    }

    template <typename Head, template <typename> typename BaseType>
    struct GenerateClasses; // can't have {}


    template <typename Head, typename Tail, template <typename> typename BaseType>
    struct GenerateClasses<Typelist<Head, Tail>, BaseType> : GenerateClasses<BaseType<Head>, BaseType>, GenerateClasses<Tail, BaseType>{

        template <typename T>
        struct Rebind
        {
            typedef BaseType<T> Result;
        };
    };


    template <typename Head, template <typename> typename BaseType>
    struct GenerateClasses<Head, BaseType> : BaseType<Head> {

        template <typename T>
        struct Rebind
        {
            typedef BaseType<T> Result;
        };
    };

    template <template <typename> typename BaseType>
    struct GenerateClasses<NullType, BaseType>{

        template <typename T> struct Rebind
        {
            typedef NullType Result;
        };
    };

    template <class T, class H>
    typename H::template Rebind<T>::Result& Field(H& obj)
    {
        return obj;
    }

    template <class T, class H>
    const typename H::template Rebind<T>::Result& Field(const H& obj)
    {
        return obj;
    }


} // end namespace TL