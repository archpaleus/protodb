#pragma once

#include <string>
#include <variant>

enum class Action {
  NONE,       // no mode provided
  ADD,        // add: add FileDescriptor data to the database
  GUESS,      // guess: guess which proto best matches a binary input.
  ENCODE,     // encode: read text, write encoded binary 
  DECODE_RAW, // decode: read binary, show encoding details
  DECODE,     // decode: read binary as proto, show proto details
  PRINT,      // print: read binary, print in parsable text format
};

struct ActionBase {
};

template<Action A>
struct ActionParams;

template<>
struct ActionParams<Action::ADD> : ActionBase {
  void Run();
};

template<>
struct ActionParams<Action::GUESS> : ActionBase {
  void Run();
};

template<>
struct ActionParams<Action::ENCODE> : ActionBase {
  void Run();
};

template<>
struct ActionParams<Action::DECODE_RAW> : ActionBase {
  void Run();

  std::string type_;
  bool dont_guess_ = false;
};

template<>
struct ActionParams<Action::DECODE> : ActionBase {
  void Run();

  std::string type_;
};

template<>
struct ActionParams<Action::PRINT> : ActionBase {
  void Run();

};

#if 0
using ActionVariant = std::variant<
  ActionParams<Action::ADD>,
  ActionParams<Action::GUESS>,
  ActionParams<Action::ENCODE>,
  ActionParams<Action::DECODE>,
  ActionParams<Action::DECODE_RAW>,
  ActionParams<Action::PRINT>,
>;
#endif