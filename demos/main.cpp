#include <exception>

#include <resource_list.hpp>

void terminate_handler()
{
  while (true) {
    // Empty: Clang deletes an infinite loop that holds only `continue;`
  }
}

int main()
{
  std::set_terminate(terminate_handler);
  initialize_platform();
  application();
}
