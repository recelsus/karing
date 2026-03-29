#include "app.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
  try {
    return karing::cli::run(argc, argv);
  } catch (const std::exception& ex) {
    std::cerr << "ERROR: " << ex.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "ERROR: unexpected failure\n";
    return 1;
  }
}
