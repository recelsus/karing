#pragma once

namespace karing::app {

class bootstrap {
public:
  bootstrap(int argc_value, char** argv_value);
  int execute();

private:
  int argc_;
  char** argv_;
};

}  // namespace karing::app
