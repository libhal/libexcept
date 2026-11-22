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

// Tests for except::unwind_frame(), the ARM EHABI frame unwinding
// instructions. Each test gives the unwinder a stack and a CPU state, runs
// some instructions, and checks the CPU state that results. The expected
// values come from section 10.3 of the EHABI:
// https://github.com/ARM-software/abi-aa/blob/main/ehabi32/ehabi32.rst#103frame-unwinding-instructions
//
// The unwinder runs on a Cortex-M only, so these tests run under QEMU. See
// tests/cortex_m/startup.cpp.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

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

/**
 * @brief A stack to unwind, and the CPU state that points into it
 *
 * The stack grows down, so the words a frame saved start at the stack pointer
 * and go up. `push()` writes words from the stack pointer upward, in the order
 * the unwinder will pop them.
 */
struct frame
{
  static constexpr std::size_t length = 300;
  static constexpr std::uintptr_t return_address = 0x0800'1001;

  std::array<std::uintptr_t, length> stack{};
  except::cortex_m_cpu cpu{};

  explicit frame(std::size_t p_stack_pointer_index = 0)
  {
    cpu.sp = &stack[p_stack_pointer_index];
    cpu.lr = return_address;
  }

  void push(std::initializer_list<std::uintptr_t> p_words)
  {
    std::ranges::copy(p_words, *cpu.sp);
  }

  void unwind(std::initializer_list<std::uint8_t> p_instructions)
  {
    except::instructions_t instructions{};
    std::ranges::copy(p_instructions, instructions.data.begin());
    except::unwind_frame(instructions, cpu);
  }

  [[nodiscard]] bool sp_is(std::size_t p_index)
  {
    return *cpu.sp == &stack[p_index];
  }
};

void finish_test()
{
  // Setup
  frame test(4);

  // Exercise
  test.unwind({ 0xB0 });

  // Verify
  expect(test.cpu.pc == frame::return_address, "finish: pc = lr");
  expect(test.sp_is(4), "finish: vsp is unchanged");
}

void vsp_add_test()
{
  {
    // Setup
    frame test(4);

    // Exercise: 00000000, vsp = vsp + 4
    test.unwind({ 0x00 });

    // Verify
    expect(test.sp_is(5), "00xxxxxx: 0x00 adds 1 word");
    expect(test.cpu.pc == frame::return_address, "00xxxxxx: pc = lr");
  }
  {
    // Setup
    frame test(4);

    // Exercise: 00111111, vsp = vsp + 0x100
    test.unwind({ 0x3F });

    // Verify
    expect(test.sp_is(68), "00xxxxxx: 0x3F adds 64 words");
  }
}

void vsp_subtract_test()
{
  {
    // Setup
    frame test(80);

    // Exercise: 01000000, vsp = vsp - 4
    test.unwind({ 0x40 });

    // Verify
    expect(test.sp_is(79), "01xxxxxx: 0x40 subtracts 1 word");
  }
  {
    // Setup
    frame test(80);

    // Exercise: 01111111, vsp = vsp - 0x100
    test.unwind({ 0x7F });

    // Verify
    expect(test.sp_is(16), "01xxxxxx: 0x7F subtracts 64 words");
  }
}

void vsp_from_register_test()
{
  // Setup
  frame test(0);
  test.cpu.r7 = &test.stack[10];

  // Exercise: 10010111, vsp = r7
  test.unwind({ 0x97 });

  // Verify
  expect(test.sp_is(10), "1001nnnn: vsp = r7");
}

void pop_r4_up_test()
{
  {
    // Setup
    frame test(0);
    test.push({ 4, 5, 6, 7 });

    // Exercise: 10100011, pop r4 to r7
    test.unwind({ 0xA3 });

    // Verify
    expect(test.cpu.r4 == 4, "10100nnn: r4");
    expect(test.cpu.r7 == 7, "10100nnn: r7");
    expect(test.sp_is(4), "10100nnn: vsp is past r7");
    expect(test.cpu.pc == frame::return_address, "10100nnn: pc = lr");
  }
  {
    // Setup
    frame test(0);
    test.push({ 4, 5, 6, 7, 0x0800'2001 });

    // Exercise: 10101011, pop r4 to r7 and lr
    test.unwind({ 0xAB });

    // Verify
    expect(test.cpu.r4 == 4, "10101nnn: r4");
    expect(test.cpu.r7 == 7, "10101nnn: r7");
    expect(test.cpu.lr == 0x0800'2001, "10101nnn: lr");
    expect(test.cpu.pc == 0x0800'2001, "10101nnn: pc = popped lr");
    expect(test.sp_is(5), "10101nnn: vsp is past lr");
  }
}

void pop_under_mask_test()
{
  {
    // Setup
    frame test(0);
    test.push({ 7, 0x0800'2001 });

    // Exercise: 10000100 00001000, pop {r7, r14}, which GCC and Clang emit at
    // -O0 (#42)
    test.unwind({ 0x84, 0x08 });

    // Verify
    expect(test.cpu.r7 == 7, "1000iiii: {r7, r14} r7");
    expect(test.cpu.lr == 0x0800'2001, "1000iiii: {r7, r14} lr");
    expect(test.cpu.pc == 0x0800'2001, "1000iiii: {r7, r14} pc = lr");
    expect(test.sp_is(2), "1000iiii: {r7, r14} vsp");
  }
  {
    // Setup
    frame test(0);
    test.push({ 4, 5, 7, 0x0800'2001 });

    // Exercise: 10000100 00001011, pop {r4, r5, r7, r14}
    test.unwind({ 0x84, 0x0B });

    // Verify
    expect(test.cpu.r4 == 4, "1000iiii: {r4, r5, r7, r14} r4");
    expect(test.cpu.r5 == 5, "1000iiii: {r4, r5, r7, r14} r5");
    expect(test.cpu.r7 == 7, "1000iiii: {r4, r5, r7, r14} r7");
    expect(test.cpu.pc == 0x0800'2001, "1000iiii: {r4, r5, r7, r14} pc");
    expect(test.sp_is(4), "1000iiii: {r4, r5, r7, r14} vsp");
  }
  {
    // Setup
    frame test(0);
    test.push({ 0x0800'3001 });

    // Exercise: 10001000 00000000, pop {r15}
    test.unwind({ 0x88, 0x00 });

    // Verify
    expect(test.cpu.pc == 0x0800'3001, "1000iiii: {r15} keeps popped pc");
    expect(test.cpu.lr == frame::return_address, "1000iiii: {r15} keeps lr");
    expect(test.sp_is(1), "1000iiii: {r15} vsp");
  }
  {
    // Setup
    frame test(0);
    test.push({ 0x0800'2001, 0x0800'3001 });

    // Exercise: 10001100 00000000, pop {r14, r15}
    test.unwind({ 0x8C, 0x00 });

    // Verify
    expect(test.cpu.lr == 0x0800'2001, "1000iiii: {r14, r15} lr");
    expect(test.cpu.pc == 0x0800'3001, "1000iiii: {r14, r15} pc is not lr");
  }
  {
    // Setup
    frame test(0);
    test.push({ reinterpret_cast<std::uintptr_t>(&test.stack[50]) });

    // Exercise: 10000010 00000000, pop {r13}, which sets vsp (#14)
    test.unwind({ 0x82, 0x00 });

    // Verify
    expect(test.sp_is(50), "1000iiii: {r13} vsp = popped r13");
  }
  {
    // Setup
    frame test(0);
    test.push(
      { 4, reinterpret_cast<std::uintptr_t>(&test.stack[50]), 0x0800'2001 });

    // Exercise: 10000110 00000001, pop {r4, r13, r14}. r14 still comes from
    // the old vsp, and the popped r13 becomes vsp afterwards (#14).
    test.unwind({ 0x86, 0x01 });

    // Verify
    expect(test.cpu.r4 == 4, "1000iiii: {r4, r13, r14} r4");
    expect(test.cpu.lr == 0x0800'2001, "1000iiii: {r4, r13, r14} lr");
    expect(test.cpu.pc == 0x0800'2001, "1000iiii: {r4, r13, r14} pc");
    expect(test.sp_is(50), "1000iiii: {r4, r13, r14} vsp = popped r13");
  }
}

void pop_r0_to_r3_test()
{
  // Setup
  frame test(0);
  test.push({ 10, 12 });

  // Exercise: 10110001 00000101, pop {r0, r2}
  test.unwind({ 0xB1, 0x05 });

  // Verify
  expect(test.cpu.r0 == 10, "10110001: r0");
  expect(test.cpu.r1 == 0, "10110001: r1 is untouched");
  expect(test.cpu.r2 == 12, "10110001: r2");
  expect(test.sp_is(2), "10110001: vsp");
}

void vsp_add_uleb128_test()
{
  {
    // Setup
    frame test(0);

    // Exercise: 10110010 00000001, vsp = vsp + 0x204 + (1 << 2)
    test.unwind({ 0xB2, 0x01 });

    // Verify
    expect(test.sp_is(130), "10110010: one-byte uleb128");
  }
  {
    // Setup
    frame test(0);

    // Exercise: 10110010 10000000 00000001, vsp = vsp + 0x204 + (128 << 2)
    test.unwind({ 0xB2, 0x80, 0x01 });

    // Verify
    expect(test.sp_is(257), "10110010: two-byte uleb128");
  }
}

void return_address_authentication_test()
{
  {
    // Setup
    frame test(0);
    test.push({ 0xAAAA'5555 });

    // Exercise: 10110100, pop the return address authentication code
    test.unwind({ 0xB4 });

    // Verify
    expect(test.sp_is(1), "10110100: vsp skips the code");
    expect(test.cpu.pc == frame::return_address, "10110100: pc = lr");
  }
  {
    // Setup
    frame test(3);

    // Exercise: 10110101, use vsp as the authentication modifier
    test.unwind({ 0xB5 });

    // Verify
    expect(test.sp_is(3), "10110101: vsp is unchanged");
    expect(test.cpu.pc == frame::return_address, "10110101: pc = lr");
  }
  {
    // Setup: what GCC 14.3 emits with -mbranch-protection=pac-ret for a
    // Cortex-M85 function that signs lr into ip, pushes {r4, ip, lr}, and
    // allocates 36 bytes below them.
    frame test(0);
    test.stack[9] = 4;
    test.stack[10] = 0xAAAA'5555;
    test.stack[11] = 0x0800'2001;

    // Exercise: vsp = vsp + 36; pop {r4}; pop the code; pop {r14}
    test.unwind({ 0x08, 0xA0, 0xB4, 0x84, 0x00 });

    // Verify
    expect(test.cpu.r4 == 4, "pac-ret: r4");
    expect(test.cpu.pc == 0x0800'2001, "pac-ret: pc");
    expect(test.sp_is(12), "pac-ret: vsp");
  }
}

void frame_pointer_sequence_test()
{
  // Setup: what Clang 23 emits with -fno-omit-frame-pointer for a function
  // that pushes {r4, r5, r7, lr}, points r7 at the saved r7, and allocates 32
  // bytes below them.
  frame test(0);
  test.cpu.r7 = &test.stack[10];
  test.stack[8] = 4;
  test.stack[9] = 5;
  test.stack[10] = 7;
  test.stack[11] = 0x0800'2001;

  // Exercise: vsp = r7; vsp = vsp - 8; pop {r4, r5, r7, r14}
  test.unwind({ 0x97, 0x41, 0x84, 0x0B });

  // Verify
  expect(test.cpu.r4 == 4, "frame pointer: r4");
  expect(test.cpu.r5 == 5, "frame pointer: r5");
  expect(test.cpu.r7 == 7, "frame pointer: r7");
  expect(test.cpu.pc == 0x0800'2001, "frame pointer: pc");
  expect(test.sp_is(12), "frame pointer: vsp");
}

#if defined(__ARM_FP)
void pop_vfp_test()
{
  {
    // Setup
    frame test(0);
    test.push({ 0x1111, 0x2222 });

    // Exercise: 11001001 10000000, pop D[8] saved by VPUSH
    test.unwind({ 0xC9, 0x80 });

    // Verify
    expect(test.cpu.d8_to_d15[0] == 0x1111, "11001001: d8 low word");
    expect(test.cpu.d8_to_d15[1] == 0x2222, "11001001: d8 high word");
    expect(test.sp_is(2), "11001001: vsp");
  }
  {
    // Setup
    frame test(0);
    test.push({ 0x1111, 0x2222, 0x3333, 0x4444 });

    // Exercise: 11010001, pop D[8] to D[9] saved by VPUSH
    test.unwind({ 0xD1 });

    // Verify
    expect(test.cpu.d8_to_d15[2] == 0x3333, "11010nnn: d9 low word");
    expect(test.sp_is(4), "11010nnn: vsp");
  }
  {
    // Setup
    frame test(0);
    test.push({ 0x1111, 0x2222 });

    // Exercise: 10110011 10000000, pop D[8] saved by FSTMFDX, which stores one
    // more word than the registers
    test.unwind({ 0xB3, 0x80 });

    // Verify
    expect(test.cpu.d8_to_d15[0] == 0x1111, "10110011: d8 low word");
    expect(test.sp_is(3), "10110011: vsp skips the extra word");
  }
  {
    // Setup
    frame test(0);

    // Exercise: 10111001, pop D[8] to D[9] saved by FSTMFDX
    test.unwind({ 0xB9 });

    // Verify
    expect(test.sp_is(5), "10111nnn: vsp skips the extra word");
  }
}
#endif
}  // namespace

int main()
{
  finish_test();
  vsp_add_test();
  vsp_subtract_test();
  vsp_from_register_test();
  pop_r4_up_test();
  pop_under_mask_test();
  pop_r0_to_r3_test();
  vsp_add_uleb128_test();
  return_address_authentication_test();
  frame_pointer_sequence_test();
#if defined(__ARM_FP)
  pop_vfp_test();
#endif

  if (failures == 0) {
    test_print("unwind_frame: all tests passed\n");
  }
  return failures;
}
