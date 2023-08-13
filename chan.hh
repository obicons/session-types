#pragma once

#include <memory>
#include <semaphore>
#include "sesstypes.hh"

template <typename T>
union SessionData;

template <typename T, HasDual P>
union SessionData<Recv<T, P>> {
    T data;
    SessionData<P> next;
};

// template <typename T>
// class ConcurrencyChan {
// public:
//     ConcurrencyChan() : messages(0) {}

//     ConcurrencyChan<T>& operator<<(T &output) {

//     }

// private:
//     std::shared_ptr<T> data;
// };