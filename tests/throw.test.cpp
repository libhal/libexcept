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

// Throws through frames that the compiler laid out, catches in main(), and
// checks that the guards in between were destroyed once each. Where
// unwind_frame.test.cpp checks each unwind instruction, these tests check
// that the whole runtime handles what a compiler emits: the index table
// lookup, each frame's unwind instructions, cleanups, and the catch.
//
// The frames are in throw_frames.cpp. Which unwind instructions they use
// depends on the compiler: Clang subtracts from vsp for runtime_sized_frame()
// and over_aligned_frame(), and GCC doesn't.
//
// The runtime runs on a Cortex-M only, so these tests run under QEMU. See
// tests/cortex_m/startup.cpp.

#include "throw_frames.hpp"

void test_print(char const* p_text);

namespace {
int failures = 0;

// Read at run time, so the compiler can't fold the size of the alloca.
unsigned volatile runtime_count = 8;
constexpr int code = 5;

void fail(char const* p_name, char const* p_reason)
{
  failures++;
  test_print("FAILED: ");
  test_print(p_name);
  test_print(": ");
  test_print(p_reason);
  test_print("\n");
}

void check(char const* p_name, int p_guards, int (*p_run)())
{
  destroyed_guards = 0;
  try {
    p_run();
  } catch (test_error const& error) {
    if (error.code != code) {
      fail(p_name, "caught the wrong code");
    } else if (destroyed_guards != p_guards) {
      fail(p_name, "wrong number of guards destroyed");
    }
    return;
  }
  fail(p_name, "nothing was thrown");
}
}  // namespace

int main()
{
  check("throw_if_positive", 0, [] {
    throw_if_positive(code);
    return 0;
  });
  check("fixed_frame", 1, [] { return fixed_frame(code); });
  check("runtime_sized_frame", 1, [] {
    return runtime_sized_frame(code, runtime_count);
  });
  check("over_aligned_frame", 1, [] { return over_aligned_frame(code); });
  check("nested_frame", 2, [] { return nested_frame(code, runtime_count); });

  if (failures == 0) {
    test_print("throw: all tests passed\n");
  }
  return failures;
}
