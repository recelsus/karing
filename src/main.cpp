#include "app/bootstrap.h"

int main(int argc, char* argv[]) {
  karing::app::bootstrap app(argc, argv);
  return app.execute();
}

