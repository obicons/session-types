#include <iostream>
#include <variant>
#include "sesstypes.hh"

using namespace std;

template <HasDual T>
void f(T a, typename T::dual b) {
    cout << "hello world" << endl;
}

template <IsNat T>
void g(T x) {}

int main() {
    Recv<int, Send<int, Z>> x;
    Send<int, Recv<int, Z>> y;
    f(x, y);

    Chan<Rec<Recv<int, Send<std::string, Send<int, Send<ostream&(ostream&), Var<Z>>>>>>> ch(&cin, &cout);
    int input = 0;

    while (input != -1) {
        auto c1 = ch.enter();
        auto c2 = c1 >> input;
        auto c3 = c2 << "You said: " << input << endl;
        auto c4 = c3.ret();
        ch = c4;
    }

    Succ<Succ<Succ<Z>>> c2;
    g(c2);

    variant<int, variant<std::string>> v = "hello world";
    cout << get<std::string>(get<std::variant<std::string>>(v)) << endl;
}
