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

// Tests for except::type_table_entry_address(), which finds the type_info
// that a catch clause's type table entry points to. The entries are full
// 32-bit offsets, so offsets with bit 30 set, more than 1 GiB, are tested
// too: the index table's 31-bit decoder would read that bit as the sign.
//
// The runtime runs on a Cortex-M only, so these tests run under QEMU. See
// tests/cortex_m/startup.cpp.

#include <array>
#include <bit>
#include <cstdint>

#include "internal.hpp"

void test_print(char const* p_text);

namespace {
int failures = 0;

void expect(bool p_passed, char const* p_name)
{
  if (not p_passed) {
    failures++;
    test_print("FAILED: ");
    test_print(p_name);
    test_print("\n");
  }
}

void type_table_entry_address_test()
{
  // Setup
  alignas(4) std::array<std::int32_t, 4> const entries = {
    0x0000'0010,
    -0x0000'0010,
    0x4000'0000,
    0x5000'0000,
  };
  auto const address_of = [&](std::size_t p_index) {
    return std::bit_cast<std::uintptr_t>(&entries[p_index]);
  };

  // Exercise
  auto const forward = except::type_table_entry_address(&entries[0]);
  auto const backward = except::type_table_entry_address(&entries[1]);
  auto const one_gibibyte = except::type_table_entry_address(&entries[2]);
  auto const more_than_one_gibibyte =
    except::type_table_entry_address(&entries[3]);

  // Verify
  expect(forward == (address_of(0) + 0x0000'0010U), "forward offset");
  expect(backward == (address_of(1) - 0x0000'0010U), "backward offset");
  expect(one_gibibyte == (address_of(2) + 0x4000'0000U), "1 GiB offset");
  expect(more_than_one_gibibyte == (address_of(3) + 0x5000'0000U),
         "offset of more than 1 GiB");
}
}  // namespace

int main()
{
  type_table_entry_address_test();

  if (failures == 0) {
    test_print("type_table: all tests passed\n");
  }
  return failures;
}
