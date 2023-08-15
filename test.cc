#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
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

template <typename... Ts, IsNat N>
struct ProtocolTypesImpl<std::variant<Ts...>, Var<N>> {
    using type = std::variant<Ts...>;
};

template <HasDual P>
using ProtocolTypes = MakeUniqueVariant<typename ProtocolTypesImpl<std::variant<>, P>::type>;

std::mutex io_lock;

void log(const std::string &tname, const std::string &action, int val) {
    io_lock.lock();
    printf("%s %s %d\n", tname.c_str(), action.c_str(), val);
    io_lock.unlock();
}

/*
 * A communication primitive for multiple threads to read/write shared data.
 * Guarantees:
 *  - No thread reads its own writes.
 *  - Every write is read.
 */
template <HasDual P>
class ConcurrentChannel {
public:
    ConcurrentChannel()
        : was_read(true), writers_waiting(0), readers_waiting(0) {}

     ConcurrentChannel(const ConcurrentChannel &other) = delete;

     ConcurrentChannel& operator=(const ConcurrentChannel &other) = delete;

    template <typename T>
    ConcurrentChannel& operator<<(const T &value) {
        std::unique_lock held_lock(lock);
        while (!was_read) {
            // Needs to be in a while loop to ignore "spurious wakeups".
            // https://en.cppreference.com/w/cpp/thread/condition_variable/wait
            writers_waiting++;
            writer_cv.wait(held_lock);
            writers_waiting--;
        }

        data = value;
        was_read = false;
        write_source = std::this_thread::get_id();

        if (readers_waiting > 0) {
            reader_cv.notify_one();
        }

        return *this;
    }

    template <typename T>
    ConcurrentChannel& operator>>(T &datum) {
        std::unique_lock held_lock(lock);
        while (write_source == std::this_thread::get_id() || was_read) {
            readers_waiting++;
            reader_cv.wait(held_lock);
            readers_waiting--;
        }

        datum = std::get<T>(data);
        was_read = true;

        if (writers_waiting > 0) {
            writer_cv.notify_one();
        }

        return *this;
    }

private:
    std::mutex lock;

    int readers_waiting;
    std::condition_variable reader_cv;

    int writers_waiting;
    std::condition_variable writer_cv;

    ProtocolTypes<P> data;
    std::thread::id write_source;
    bool was_read;
};

template <HasDual P,
          typename CommunicationMedium,
          std::invocable<Chan<P, CommunicationMedium, CommunicationMedium>> F,
          std::invocable<Chan<typename P::dual, CommunicationMedium, CommunicationMedium>> G>
std::pair<std::thread, std::thread> connect(F f, G g, CommunicationMedium chan) {
    Chan<P, CommunicationMedium, CommunicationMedium> c1(chan, chan);
    Chan<typename P::dual, CommunicationMedium, CommunicationMedium> c2(chan, chan);
    return {std::thread(f, c1), std::thread(g, c2)};
}

using Protocol = Rec<Send<int, Recv<int, Var<Z>>>>;

struct {
    template <typename CommunicationMedium>
    void operator()(Chan<Protocol, CommunicationMedium, CommunicationMedium> chan) {
        int val;

        auto c = chan.enter();
        for (int i = 0; i < 5; i++) {
            auto c1 = c << i;
            log("T1", "sent", i);

            int val;
            auto c2 = c1 >> val;
            log("T1", "received", val);

            c = c2.ret().enter();
        }
        log("T1", "done", -1);
    }
} t1;

struct {
    template <typename CommunicationMedium>
    void operator()(Chan<Protocol::dual, CommunicationMedium, CommunicationMedium> chan) {
        auto c = chan.enter();
        for (int i = 0; i < 5; i++) {
            int val;
            auto c1 = c >> val;
            log("T2", "received", val);

            auto c2 = c1 << val * 2;
            log("T2", "sent", val * 2);

            c = c2.ret().enter();
        }
        log("T2", "done", -1);
    }
} t2;

auto t1_v3 = [](auto chan) {
        auto c = chan.enter();
        int x;
        for (int i = 0; i < 5; i++) {
            auto c1 = c << i;
            log("T1", "sent", i);

            int val;
            auto c2 = c1 >> val;
            log("T1", "received", val);

            c = c2.ret().enter();
        }
        log("T1", "done", -1);
};

// template <typename CommunicationMedium>
// void t1(Chan<Protocol, CommunicationMedium, CommunicationMedium> chan) {
//     std::cout << "t1: " << std::this_thread::get_id() << std::endl;
//     auto c = chan.enter();
//     for (int i = 0; i < 5; i++) {
//         auto c1 = c << i;
//         log("T1", "sent", i);

//         int val;
//         auto c2 = c1 >> val;
//         log("T1", "received", val);

//         c = c2.ret().enter();
//     }
//     log("T1", "done", -1);
// }

// template <typename CommunicationMedium>
// void t2(Chan<Protocol::dual, CommunicationMedium, CommunicationMedium> chan) {
//     std::cout << "t2: " << std::this_thread::get_id() << std::endl;
//     auto c = chan.enter();
//     for (int i = 0; i < 5; i++) {
//         int val;
//         auto c1 = c >> val;
//         log("T2", "received", val);

//         auto c2 = c1 << val * 2;
//         log("T2", "sent", val * 2);

//         c = c2.ret().enter();
//     }
//     log("T2", "done", -1);
// }

int main() {
    auto chan = std::make_shared<ConcurrentChannel<Protocol>>();
    auto threads = connect<Protocol>(t1, t2, chan);
    threads.first.join();
    threads.second.join();
    //ch >> val;

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