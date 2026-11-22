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

#pragma once

// The functions that the throw tests unwind through. They are in their own
// file, which includes no standard headers, so a test can build them with a
// different compiler from the rest of the program.

/**
 * @brief The exception the frames throw
 */
struct test_error
{
  /// The code passed down to throw_if_positive()
  int code;
};

/**
 * @brief The number of guard destructors that have run
 *
 * Every frame below, except throw_if_positive(), holds one guard.
 */
extern int destroyed_guards;

/**
 * @brief Throw test_error{ p_code } if p_code is positive
 *
 * The throw is the last thing in the function, so with Clang at -Os the
 * function ends with `bl __cxa_throw`, and its return address is the first
 * byte of the next function.
 *
 * @param p_code - code to throw
 */
void throw_if_positive(int p_code);

/**
 * @brief A frame with a fixed-size local buffer
 *
 * @param p_code - code to throw
 * @return a value read from the buffer, never reached when p_code > 0
 */
int fixed_frame(int p_code);

/**
 * @brief A frame that allocates its buffer with alloca
 *
 * sp moves by an amount known only at run time, so the compiler keeps a frame
 * pointer. Clang describes the frame with `vsp = r7`, then `vsp = vsp - N`.
 *
 * @param p_code - code to throw
 * @param p_count - number of ints to allocate, at least 2
 * @return a value read from the buffer, never reached when p_code > 0
 */
int runtime_sized_frame(int p_code, unsigned p_count);

/**
 * @brief A frame with a local aligned to 32 bytes
 *
 * Clang rounds sp down to 32 bytes at run time, keeps a frame pointer, and
 * describes the frame with `vsp = r7`, then `vsp = vsp - N`.
 *
 * @param p_code - code to throw
 * @return a value read from the buffer, never reached when p_code > 0
 */
int over_aligned_frame(int p_code);

/**
 * @brief A frame with a guard that calls runtime_sized_frame()
 *
 * Unwinding goes through two frames that each hold a guard.
 *
 * @param p_code - code to throw
 * @param p_count - passed to runtime_sized_frame()
 * @return a value from runtime_sized_frame(), never reached when p_code > 0
 */
int nested_frame(int p_code, unsigned p_count);
