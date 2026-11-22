// Copyright 2026 Khalil Estell and the libhal contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Startup code for the tests that run on a Cortex-M under QEMU. Output and the
// exit code go through semihosting, so the tests need no peripherals and run
// on any QEMU machine with the right core.

#include <cstdint>
#include <exception>

// NOLINTBEGIN(bugprone-reserved-identifier)
// NOLINTBEGIN(readability-identifier-naming)
extern "C"
{
  extern std::uint32_t __bss_start__[];
  extern std::uint32_t __bss_end__[];
  extern std::uint32_t __data_start__[];
  extern std::uint32_t __data_end__[];
  extern std::uint32_t __data_load[];
  extern std::uint32_t __stack_top[];
  extern void (*__init_array_start[])();
  extern void (*__init_array_end[])();
}
// NOLINTEND(readability-identifier-naming)
// NOLINTEND(bugprone-reserved-identifier)

int main();

namespace {
constexpr std::uint32_t sys_write0 = 0x04;
constexpr std::uint32_t sys_exit_extended = 0x20;
constexpr std::uint32_t adp_stopped_application_exit = 0x2'0026;

std::uint32_t semihost(std::uint32_t p_operation, void const* p_argument)
{
  register std::uint32_t operation asm("r0") = p_operation;
  register void const* argument asm("r1") = p_argument;
  asm volatile("bkpt 0xAB" : "+r"(operation) : "r"(argument) : "memory");
  return operation;
}

[[noreturn]] void exit_with(int p_code)
{
  std::uint32_t const block[2] = { adp_stopped_application_exit,
                                   static_cast<std::uint32_t>(p_code) };
  semihost(sys_exit_extended, block);
  while (true) {
    asm volatile("");
  }
}

// A spare or reserved unwind instruction calls std::terminate(). This handler
// fails the test run instead of halting it.
[[noreturn]] void terminate_handler()
{
  semihost(sys_write0, "std::terminate() was called\n");
  exit_with(-2);
}

}  // namespace

extern "C" [[noreturn]] void reset_handler()
{
#if defined(__ARM_FP)
  // Give CP10 and CP11 (the FPU) full access, through CPACR
  auto* cpacr = reinterpret_cast<std::uint32_t volatile*>(0xE000'ED88);
  *cpacr = *cpacr | (0b1111U << 20U);
  asm volatile("dsb\nisb" ::: "memory");
#endif
  auto const* source = __data_load;
  for (auto* word = __data_start__; word < __data_end__; word++) {
    *word = *source;
    source++;
  }
  for (auto* word = __bss_start__; word < __bss_end__; word++) {
    *word = 0;
  }
  for (auto* function = __init_array_start; function < __init_array_end;
       function++) {
    (*function)();
  }
  std::set_terminate(terminate_handler);
  exit_with(main());
}

namespace {
void fault_handler()
{
  semihost(sys_write0, "fault\n");
  exit_with(-1);
}

[[gnu::section(".vectors"), gnu::used]] void const* const vector_table[] = {
  __stack_top,
  reinterpret_cast<void const*>(reset_handler),
  reinterpret_cast<void const*>(fault_handler),  // NMI
  reinterpret_cast<void const*>(fault_handler),  // HardFault
  reinterpret_cast<void const*>(fault_handler),  // MemManage
  reinterpret_cast<void const*>(fault_handler),  // BusFault
  reinterpret_cast<void const*>(fault_handler),  // UsageFault
};
}  // namespace

/**
 * @brief Print a null-terminated string to the host's console
 *
 * @param p_text - text to print
 */
void test_print(char const* p_text)
{
  semihost(sys_write0, p_text);
}

// NOLINTBEGIN(bugprone-reserved-identifier)
// NOLINTBEGIN(readability-identifier-naming)
extern "C"
{
  void _init()
  {
  }
  void _fini()
  {
  }
}
// NOLINTEND(readability-identifier-naming)
// NOLINTEND(bugprone-reserved-identifier)
