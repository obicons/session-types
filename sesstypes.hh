#pragma once

#include <concepts>
#include <iostream>
#include <type_traits>

template <typename T>
concept HasDual = requires { typename T::dual; };

struct Z {
    using dual = Z;
};

template <typename T>
struct Succ;

template <typename T>
struct IsNatImpl : std::false_type {};

template <>
struct IsNatImpl<Z> : std::true_type {};

template <typename T>
struct IsNatImpl<Succ<T>>
    : std::conditional_t<
                IsNatImpl<T>::value,
                std::true_type,
                std::false_type
      > {};

template <typename T>
concept IsNat = IsNatImpl<T>::value;

template <>
struct Succ<Z> {};

template <IsNat T>
struct Succ<T> {};

template <typename T, HasDual P>
struct Send;

template <typename T, HasDual P>
struct Recv {
    using dual = Send<T, typename P::dual>;
};

template <typename T, HasDual P>
struct Send {
    using dual = Recv<T, typename P::dual>;
};

template <HasDual P>
struct Rec {
    using dual = Rec<typename P::dual>;
};

template <IsNat T>
struct Var {
    using dual = Var<T>;
};

struct ChannelReusedError {};

template <typename IT, typename OT>
class ChanBase {
public:
    ChanBase(IT input, OT output)
        : input(input), output(output), used(false) {}

protected:
    IT input;
    OT output;
    bool used;
};

template <HasDual P, typename IT, typename OT, typename E=Z>
class Chan : ChanBase<IT, OT> {
public:
    using ChanBase<IT, OT>::ChanBase;
};

template <typename T, HasDual P, typename IT, typename OT, typename E>
class Chan<Recv<T, P>, IT, OT, E> : ChanBase<IT, OT> {
public:
    using ChanBase<IT, OT>::ChanBase;

    Chan<P, IT, OT, E> operator>>(T &t) {
        if (ChanBase<IT, OT>::used) {
            throw ChannelReusedError();
        }

        ChanBase<IT, OT>::used = true;
        (*ChanBase<IT, OT>::input) >> t;
        return Chan<P, IT, OT, E>(ChanBase<IT, OT>::input, ChanBase<IT, OT>::output);
    }
};

template <typename T, HasDual P, typename IT, typename OT, typename E>
class Chan<Send<T, P>, IT, OT, E> : ChanBase<IT, OT> {
public:
    using ChanBase<IT, OT>::ChanBase;

    Chan<P, IT, OT, E> operator<<(const T &t) {
        if (ChanBase<IT, OT>::used) {
            throw ChannelReusedError();
        }

        ChanBase<IT, OT>::used = true;
        (*ChanBase<IT, OT>::output) << t;
        return Chan<P, IT, OT, E>(ChanBase<IT, OT>::input, ChanBase<IT, OT>::output);
    }
};

template <HasDual P, typename IT, typename OT, typename E>
class Chan<Rec<P>, IT, OT, E> : ChanBase<IT, OT> {
public:
    using ChanBase<IT, OT>::ChanBase;

    Chan<P, IT, OT, std::pair<Rec<P>, E>> enter() {
        if (ChanBase<IT, OT>::used) {
            throw ChannelReusedError();
        }

        ChanBase<IT, OT>::used = true;
        return Chan<P, IT, OT, std::pair<Rec<P>, E>>(ChanBase<IT, OT>::input, ChanBase<IT, OT>::output);
    }
};

template <HasDual P, typename IT, typename OT, typename E>
class Chan<Var<Z>, IT, OT, std::pair<P, E>> : ChanBase<IT, OT> {
public:
    using ChanBase<IT, OT>::ChanBase;

    Chan<P, IT, OT, E> ret() {
        if (ChanBase<IT, OT>::used) {
            throw ChannelReusedError();
        }

        ChanBase<IT, OT>::used = true;
        return Chan<P, IT, OT, E>(ChanBase<IT, OT>::input, ChanBase<IT, OT>::output);
    }
};

template <typename T, HasDual P, typename IT, typename OT, typename E>
class Chan<Var<Succ<T>>, IT, OT, std::pair<P, E>> : ChanBase<IT, OT> {
public:
    using ChanBase<IT, OT>::ChanBase;

    Chan<Var<T>, IT, OT, E> ret() {
        if (ChanBase<IT, OT>::used) {
            throw ChannelReusedError();
        }

        ChanBase<IT, OT>::used = true;
        return Chan<P, IT, OT, E>(ChanBase<IT, OT>::input, ChanBase<IT, OT>::output);
    }
};
