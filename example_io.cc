#include <iostream>
#include <random>
#include "sesstypes.hh"

using ExitUserLost = Send<std::string, Send<int, Send<std::string, Send<std::ostream&(std::ostream&), Z>>>>;
using ExitUserWon = Send<std::string, Send<std::ostream&(std::ostream &), Z>>;

using ExitProtocol = Choose<ExitUserLost, ExitUserWon>;

using KeepPlayingProtocol = Send<std::string, Recv<std::string, Var<Z>>>;

template <HasDual P>
using QueryUserProtocol = Send<std::string, Recv<int, P>>;

using GuessingGameProtocol =
    Rec<Choose<QueryUserProtocol<Choose<KeepPlayingProtocol, Var<Z>>>,
               ExitProtocol>>;

int main() {
  std::default_random_engine generator;
  generator.seed(time(nullptr));

  std::uniform_int_distribution distribution(1,10);
  const auto the_number = distribution(generator);

  auto keep_going = true;
  auto guess = 0;

  Chan<GuessingGameProtocol, decltype(&std::cin), decltype(&std::cout)> chan(&std::cin, &std::cout);

  while (keep_going) {
    auto c1 = chan.enter().choose1();
    auto c2 = c1 << "Guess: ";
    auto c3 = c2 >> guess;

    keep_going = guess != the_number;
    if (keep_going) {
      auto c4 = c3.choose1();
      auto c5 = c4 << "Incorrect. Keep playing? (y/n) ";
      std::string response;
      auto c6 = c5 >> response;
      keep_going = response != "n";

      chan = c6.ret();
    } else {
      chan = c3.choose2().ret();
    }
  }

  if (guess != the_number) {
    auto ce = chan.enter().choose2().choose1();
    ce << "You lose. I was thinking of " << the_number << "." << std::endl;
  } else {
    auto ce = chan.enter().choose2().choose2();    
    ce << "You win!" << std::endl;
  }
}

// int main() {
//     using Protocol = Rec<Recv<int, Send<std::string, Send<int, Send<ostream&(ostream&), Var<Z>>>>>>;
//     Chan<Protocol, decltype(&cin), decltype(&cout)> ch(&cin, &cout);
//     int input = 0;

//     while (input != -1) {
//         auto c1 = ch.enter();
//         auto c2 = c1 >> input;
//         auto c3 = c2 << "You said: " << input << endl;
//         auto c4 = c3.ret();
//         ch = c4;
//     }
// }
