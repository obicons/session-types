#include <iostream>
#include <memory>
#include <semaphore>
#include <type_traits>
#include <tuple>
#include <variant>
#include "sesstypes.hh"

template <typename T, typename... Ts>
struct Unique : std::type_identity<T> {};

template <typename... Ts, typename U, typename... Us>
struct Unique<std::variant<Ts...>, U, Us...>
    : std::conditional_t<(std::is_same_v<U, Ts> || ...),
                         Unique<std::variant<Ts...>, Us...>,
                         Unique<std::variant<Ts..., U>, Us...>> {};

template <typename T>
struct MakeUniqueVariantImpl;

template <typename... Ts>
struct MakeUniqueVariantImpl<std::variant<Ts...>> {
    using type = typename Unique<std::variant<>, Ts...>::type;
};

template <typename T>
using MakeUniqueVariant = typename MakeUniqueVariantImpl<T>::type;

template <typename... Ts>
using UniqueVariant = typename Unique<std::variant<>, Ts...>::type;

// We don't need to rename if we just use std::variant from the start.
template<typename A, template<typename...> typename B> struct RenameImpl;

template<template<typename...> typename A, typename... T, template<typename...> typename B>
struct RenameImpl<A<T...>, B> {
    using type = B<T...>;
};

template<typename A, template<typename...> typename B>
using RenameType = typename RenameImpl<A, B>::type;

template<template<typename...> typename OldType, template<typename...> typename NewType, typename T>
struct RenameAllOccurencesImpl;

template<template<typename...> typename OldType,
         template<typename...> typename NewType,
         typename ...Ts>
struct RenameAllOccurencesImpl<OldType, NewType, OldType<Ts...>> {
    using type = NewType<typename RenameAllOccurencesImpl<OldType, NewType, Ts>::type...>;
};

template<template<typename...> typename OldType,
         template<typename...> typename NewType,
         template<typename...> typename ContainerType,
         typename ...Ts>
struct RenameAllOccurencesImpl<OldType, NewType, ContainerType<Ts...>> {
    using type = ContainerType<typename RenameAllOccurencesImpl<OldType, NewType, Ts>::type...>;
};

template<template<typename...> typename OldType,
         template<typename...> typename NewType,
         typename T>
struct RenameAllOccurencesImpl {
    using type = T;
};

template<template<typename...> typename OldType,
         template<typename...> typename NewType,
         typename T>
using RenameAllOccurences = typename RenameAllOccurencesImpl<OldType, NewType, T>::type;

template<typename... Ts> struct CatImpl;

template<typename... Ts>
struct CatImpl<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
};

template<typename... Ts, typename... Us, typename... Vs>
struct CatImpl<std::tuple<Ts...>, std::tuple<Us...>, Vs...> {
    using type = typename CatImpl<std::tuple<Ts..., Us...>, Vs...>::type;
};

template<typename... Ts>
using Cat = typename CatImpl<Ts...>::type;

template<typename... Ts> struct FlattenImpl;

template<typename... T1, typename... T2, typename... Ts>
struct FlattenImpl<std::tuple<T1...>, std::tuple<T2...>,  Ts...> {
    using type = Cat<typename std::tuple<T1...>,
                     typename FlattenImpl<std::tuple<>, T2...>::type,
                     typename FlattenImpl<std::tuple<>, Ts...>::type>;
};

template<typename... T1, typename T2, typename... Ts>
struct FlattenImpl<std::tuple<T1...>, T2, Ts...> {
    using type = Cat<typename std::tuple<T1...>,
                     std::tuple<T2>,
                     typename FlattenImpl<std::tuple<>, Ts...>::type>;
};

template<typename... Ts>
struct FlattenImpl<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
};

template <typename A>
struct FlattenOuter;

template <template <typename...> typename A, typename ...Ts>
struct FlattenOuter<A<Ts...>> {
    using type = RenameType<
        typename FlattenImpl<std::tuple<>, RenameAllOccurences<A, std::tuple, A<Ts...>>>::type,
        A
    >;
};

template <typename T>
using Flatten = typename FlattenOuter<T>::type;

template <typename Variant, typename T>
struct ProtocolTypesImpl;

template <typename... Ts, typename T, HasDual P>
struct ProtocolTypesImpl<std::variant<Ts...>, Recv<T, P>> {
    using type = typename ProtocolTypesImpl<std::variant<T, Ts...>, P>::type;
};

template <typename... Ts>
struct ProtocolTypesImpl<std::variant<Ts...>, Z> {
    using type = std::variant<Z, Ts...>;
};

template <typename... Ts, typename T, HasDual P>
struct ProtocolTypesImpl<std::variant<Ts...>, Send<T, P>> {
    using type = typename ProtocolTypesImpl<std::variant<T, Ts...>, P>::type;
};

template <typename... Ts, HasDual P>
struct ProtocolTypesImpl<std::variant<Ts...>, Rec<P>> {
    using type = typename ProtocolTypesImpl<std::variant<Ts...>, P>::type;
};

template <HasDual P>
using ProtocolTypes = MakeUniqueVariant<typename ProtocolTypesImpl<std::variant<>, P>::type>;

template <HasDual P>
class ConcurrentChannel {
public:
    ConcurrentChannel() :
        available_reads(0),
        available_writes(1) {}

    template <typename T>
    ConcurrentChannel& operator<<(const T &value) {
        available_writes.acquire();
        data = value;
        available_reads.release();
        return *this;
    }

    template <typename T>
    ConcurrentChannel& operator>>(T &datum) {
        available_reads.acquire();
        datum = std::get<T>(data);
        available_writes.release();
        return *this;
    }

private:
    std::counting_semaphore<1> available_reads;
    std::counting_semaphore<1> available_writes;
    ProtocolTypes<P> data;
};

int main() {
    ProtocolTypes<Rec<Send<std::string, Recv<int, Recv<double, Recv<double, Z>>>>>> x;

    ConcurrentChannel<Rec<Send<std::string, Recv<int, Recv<double, Recv<double, Z>>>>>> ch;
    ch << "hello world";

    std::string val;
    ch >> val;
    std::cout << "Read: " << val << std::endl;
    ch >> val;

    // UniqueVariant<int, char, int, char, float, char, std::string> v;
    // v = "hello world";
    // std::cout << std::get<std::string>(v) << std::endl;

    // Cat<std::tuple<int, long>, std::tuple<std::string, char>, std::tuple<double>> c;
    // static_assert(std::is_same_v<decltype(c), std::tuple<int, long, std::string, char, double>>);

    // Flatten<std::tuple<>> c2;
    // static_assert(std::is_same_v<decltype(c2), std::tuple<>>);

    // Flatten<std::variant<int>> c3;
    // static_assert(std::is_same_v<decltype(c3), std::variant<int>>);

    // Flatten<std::variant<int, double>> c4;
    // static_assert(std::is_same_v<decltype(c4), std::variant<int, double>>);

    // Flatten<std::variant<int, std::variant<double>>> c5;
    // static_assert(std::is_same_v<decltype(c5), std::variant<int, double>>);

    // Flatten<std::variant<int, std::variant<std::variant<double>>>> c6;
    // static_assert(std::is_same_v<decltype(c6), std::variant<int, double>>);

    // Flatten<std::variant<std::variant<std::variant<int>>>> c7;
    // static_assert(std::is_same_v<decltype(c7), std::variant<int>>);
}