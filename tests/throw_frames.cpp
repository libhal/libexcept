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

#include "throw_frames.hpp"

int destroyed_guards = 0;

namespace {
struct guard
{
  guard() = default;
  guard(guard const&) = delete;
  guard& operator=(guard const&) = delete;
  guard(guard&&) = delete;
  guard& operator=(guard&&) = delete;
  ~guard()
  {
    destroyed_guards++;
  }
};

[[gnu::noinline]] void fill(int* p_buffer, unsigned p_count)
{
  for (unsigned i = 0; i < p_count; i++) {
    p_buffer[i] = static_cast<int>(i);
  }
}
}  // namespace

// Each frame passes p_code through buffer[1], which is 1, so the compiler
// can't move the call to throw_if_positive() ahead of fill().

[[gnu::noinline]] void throw_if_positive(int p_code)
{
  if (p_code > 0) {
    throw test_error{ p_code };
  }
}

[[gnu::noinline]] int fixed_frame(int p_code)
{
  guard const scope_guard;
  int buffer[8];
  fill(buffer, 8);
  throw_if_positive((p_code + buffer[1]) - 1);
  return buffer[p_code & 0b111];
}

[[gnu::noinline]] int runtime_sized_frame(int p_code, unsigned p_count)
{
  guard const scope_guard;
  auto* buffer = static_cast<int*>(__builtin_alloca(p_count * sizeof(int)));
  fill(buffer, p_count);
  throw_if_positive((p_code + buffer[1]) - 1);
  return buffer[static_cast<unsigned>(p_code) % p_count];
}

[[gnu::noinline]] int over_aligned_frame(int p_code)
{
  guard const scope_guard;
  alignas(32) int buffer[8];
  fill(buffer, 8);
  throw_if_positive((p_code + buffer[1]) - 1);
  return buffer[p_code & 0b111];
}

[[gnu::noinline]] int nested_frame(int p_code, unsigned p_count)
{
  guard const scope_guard;
  return runtime_sized_frame(p_code, p_count) + 1;
}
