#include <cstdio>
#include <iostream>
#include <memory>
#include "sesstypes.hh"
#include "concurrentmedium.hh"

using Protocol = Rec<Send<int, Recv<int, Var<Z>>>>;

void log(const std::string &tname, const std::string &action, int val) {
    printf("%s %s %d\n", tname.c_str(), action.c_str(), val);
}

void log(const std::string &tname, const std::string &action) {
    printf("%s %s\n", tname.c_str(), action.c_str());
}

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
        log("T2", "done");
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
        log("T1", "done");
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
    auto chan = std::make_shared<ConcurrentMedium<ProtocolTypes<Protocol>>>();
    auto threads = connect<Protocol>(t1, t2, chan);
    threads.first.join();
    threads.second.join();
}
