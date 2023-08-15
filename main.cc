#include <iostream>
#include <variant>
#include "sesstypes.hh"

using namespace std;

int main() {
    Chan<
        Rec<Recv<int, Send<std::string, Send<int, Send<ostream&(ostream&), Var<Z>>>>>>,
        decltype(&cin),
        decltype(&cout)> ch(&cin, &cout);
    int input = 0;

    while (input != -1) {
        auto c1 = ch.enter();
        auto c2 = c1 >> input;
        auto c3 = c2 << "You said: " << input << endl;
        auto c4 = c3.ret();
        ch = c4;
    }
}
