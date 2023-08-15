#pragma once

#include <concepts>
#include <iostream>

#define CHAN_BASE                                           \
    public:                                                 \
        Chan(IT input, OT output)                           \
            : input(input), output(output), used(false) {}  \
    private:                                                \
        IT input;                                           \
        OT output;                                          \
        bool used;

template <typename T>
concept HasDual = requires { typename T::dual; };

struct Z {
    using dual = Z;
};

template <typename T>
struct Succ;

template <typename T>
struct IsNatImpl;

template <>
struct IsNatImpl<Z> {
    using value = std::true_type;
};

template <typename T>
struct IsNatImpl {
    using value = std::false_type;
};

template <typename T>
struct IsNatImpl<Succ<T>> {
    using value = typename IsNatImpl<T>::value;
};

template <typename T>
concept IsNat = std::same_as<typename IsNatImpl<T>::value, std::true_type>;

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

template <HasDual P, typename IT, typename OT, typename E=Z>
class Chan {
    CHAN_BASE
};

template <typename T, HasDual P, typename IT, typename OT, typename E>
class Chan<Recv<T, P>, IT, OT, E> {
    CHAN_BASE

public:
    Chan<P, IT, OT, E> operator>>(T &t) {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        (*input) >> t;
        return Chan<P, IT, OT, E>(input, output);
    }
};

template <typename T, HasDual P, typename IT, typename OT, typename E>
class Chan<Send<T, P>, IT, OT, E> {
    CHAN_BASE

public:
    Chan<P, IT, OT, E> operator<<(const T &t) {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        (*output) << t;
        return Chan<P, IT, OT, E>(input, output);
    }
};

template <HasDual P, typename IT, typename OT, typename E>
class Chan<Rec<P>, IT, OT, E> {
    CHAN_BASE

public:
    Chan<P, IT, OT, std::pair<Rec<P>, E>> enter() {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        return Chan<P, IT, OT, std::pair<Rec<P>, E>>(input, output);
    }
};

template <HasDual P, typename IT, typename OT, typename E>
class Chan<Var<Z>, IT, OT, std::pair<P, E>> {
    CHAN_BASE

public:
    Chan<P, IT, OT, E> ret() {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        return Chan<P, IT, OT, E>(input, output);
    }
};

template <typename T, HasDual P, typename IT, typename OT, typename E>
class Chan<Var<Succ<T>>, IT, OT, std::pair<P, E>> {
    CHAN_BASE

public:
    Chan<Var<T>, IT, OT, E> ret() {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        return Chan<P, IT, OT, E>(input, output);
    }
};
