#pragma once

#include <concepts>
#include <iostream>

#define CHAN_BASE                                           \
    public:                                                 \
        Chan(std::istream *input, std::ostream *output)     \
            : input(input), output(output), used(false) {}  \
    private:                                                \
        std::istream *input;                                \
        std::ostream *output;                               \
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

template <HasDual P, typename E=Z>
class Chan {
    CHAN_BASE
};

template <typename T, HasDual P, typename E>
class Chan<Recv<T, P>, E> {
    CHAN_BASE

public:
    Chan<P, E> operator>>(T &t) {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        std::istream &new_input = (*input) >> t;
        return Chan<P, E>(&new_input, output);
    }
};

template <typename T, HasDual P, typename E>
class Chan<Send<T, P>, E> {
    CHAN_BASE

public:
    Chan<P, E> operator<<(const T &t) {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        std::ostream &new_output = (*output) << t;
        return Chan<P, E>(input, &new_output);
    }
};

template <HasDual P, typename E>
class Chan<Rec<P>, E> {
    CHAN_BASE

public:
    Chan<P, std::pair<Rec<P>, E>> enter() {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        return Chan<P, std::pair<Rec<P>, E>>(input, output);
    }
};

template <HasDual P, typename E>
class Chan<Var<Z>, std::pair<P, E>> {
    CHAN_BASE

public:
    Chan<P, E> ret() {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        return Chan<P, E>(input, output);
    }
};

template <typename T, HasDual P, typename E>
class Chan<Var<Succ<T>>, std::pair<P, E>> {
    CHAN_BASE

public:
    Chan<Var<T>, E> ret() {
        if (used) {
            throw ChannelReusedError();
        }

        used = true;
        return Chan<P, E>(input, output);
    }
};
