#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
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

template <typename T, typename V>
struct OneOf : public std::false_type {};

template <typename T, typename... Ts>
struct OneOf<T, std::variant<Ts...>> : public std::conditional_t<
        (std::is_same_v<T, Ts> || ...),
        std::true_type,
        std::false_type
    >
{};

template <typename T, typename V>
concept AssignableToVariant = OneOf<T, V>::value;

/*
 * A communication medium for multiple threads to read/write shared data.
 *
 * Allows multiple threads to communicate objects in a variant.
 *
 * Guarantees:
 *  - No thread reads its own writes.
 *  - Every write is read.
 */
template <typename T>
class ConcurrentMedium;

template <typename... Ts>
class ConcurrentMedium<std::variant<Ts...>> {
public:
    ConcurrentMedium()
        : was_read(true), writers_waiting(0), readers_waiting(0) {}

     ConcurrentMedium(const ConcurrentMedium &other) = delete;

     ConcurrentMedium& operator=(const ConcurrentMedium &other) = delete;

    template <AssignableToVariant<std::variant<Ts...>> T>
    ConcurrentMedium& operator<<(const T &value) {
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

    template <AssignableToVariant<std::variant<Ts...>> T>
    ConcurrentMedium& operator>>(T &datum) {
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

    std::variant<Ts...> data;
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
