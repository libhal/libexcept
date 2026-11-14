// Copyright 2024 - 2025 Khalil Estell and the libhal contributors
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

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <typeinfo>

#include <libhal-util/bit.hpp>

namespace except {
using exception_ptr = void*;
using destructor_t = void(void*);

// TODO(#102): use uintptr and intptr instead of exact word length types.
struct register_t
{
  std::uintptr_t data;

  register_t()
    : data(0)
  {
  }

  register_t(std::uintptr_t p_data)
    : data(p_data)
  {
  }

  register_t(void const* p_data)
    : data(reinterpret_cast<std::uintptr_t>(p_data))
  {
  }

  operator std::uint32_t() const
  {
    return data;
  }

  auto* operator*()
  {
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<std::uintptr_t*>(data);
  }
};

namespace arm_ehabi {
constexpr std::uint8_t finish = 0xb0;
constexpr std::uint32_t end_descriptor = 0x0000'0000;
constexpr std::uint32_t su16_mask = 0b1111'1111'1111'1110;
}  // namespace arm_ehabi

constexpr std::uintptr_t to_absolute_address(void const* p_object)
{
  auto const object_address = std::bit_cast<std::uintptr_t>(p_object);
  auto offset = *std::bit_cast<std::int32_t const*>(p_object);

  // Shift bits to the end
  offset <<= 1;
  // Arithmetic shift to the right to sign extend.
  offset >>= 1;

  auto const final_address = object_address + offset;
  return static_cast<std::uint32_t>(final_address);
}

constexpr std::uint32_t* to_absolute_address_ptr(void const* p_object)
{
  return std::bit_cast<std::uint32_t*>(to_absolute_address(p_object));
}

/**
 * @brief Returns true if a word is a compact model header
 *
 * An exception table entry's first word is either a compact model header, with
 * bit 31 set, or the prel31 address of a generic model personality routine,
 * such as `__gxx_personality_v0`, with bit 31 clear. The word after a generic
 * routine's address holds its extra word count in bits 31:24, which never
 * reaches 128, so bit 31 tells the two apart there too.
 *
 * @param p_header - the first word of a function's unwind instructions
 * @return true if p_header is a compact model header
 */
[[gnu::always_inline]] [[nodiscard]] constexpr bool is_compact_model(
  std::uint32_t p_header)
{
  constexpr auto compact_model = hal::bit_mask::from<31>();
  return hal::bit_extract<compact_model>(p_header) == 1;
}

/**
 * @brief Returns the number of words after the first that hold a function's
 * unwind instructions
 *
 * - Compact model, personality routine 0: none.
 * - Compact model, personality routines 1 and 2: bits 23:16.
 * - Generic model: bits 31:24 of the word after the personality routine's
 *   address.
 *
 * @param p_header - the first word of a function's unwind instructions
 * @return the number of extra words
 */
[[gnu::always_inline]] [[nodiscard]] constexpr std::uint32_t unwind_word_count(
  std::uint32_t p_header)
{
  constexpr auto personality_index = hal::bit_mask::from<24, 27>();
  constexpr auto compact_count = hal::bit_mask::from<16, 23>();
  constexpr auto generic_count = hal::bit_mask::from<24, 31>();

  if (not is_compact_model(p_header)) {
    return hal::bit_extract<generic_count>(p_header);
  }
  if (hal::bit_extract<personality_index>(p_header) == 0) {
    return 0;
  }
  return hal::bit_extract<compact_count>(p_header);
}

struct function_t
{
  using callable_t = void();

  std::uintptr_t address;

  function_t(std::uintptr_t p_address)
    : address(p_address)
  {
  }

  operator std::uintptr_t()
  {
    return reinterpret_cast<std::uintptr_t>(address);
  }

  operator void*()
  {
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    return reinterpret_cast<void*>(address);
  }

  bool operator<(function_t const& p_other) const
  {
    return address < p_other.address;
  }

  bool operator==(function_t const& p_other) const
  {
    return address == p_other.address;
  }
};

struct index_entry_t
{
  std::uint32_t function_offset;
  std::uint32_t personality_offset;

  [[gnu::always_inline]] [[nodiscard]] constexpr bool has_inlined_personality()
    const
  {
    // 31st bit is `1` when the personality/unwind information is inlined, other
    // otherwise, personality_offset is an offset.
    constexpr auto mask = hal::bit_mask::from<31>();
    return hal::bit_extract<mask>(personality_offset);
  }

  [[gnu::always_inline]] [[nodiscard]] constexpr bool is_noexcept() const
  {
    return personality_offset == 0x1;
  }

  [[gnu::always_inline]] [[nodiscard]] constexpr std::uint32_t const*
  personality() const
  {
    return to_absolute_address_ptr(&personality_offset);
  }

  /**
   * @brief Returns the pointer to a generic model entry's LSDA
   *
   * The LSDA follows the unwind instructions: the word after the personality
   * routine's address, then the number of extra words in its bits 31:24.
   *
   * @param p_personality - pointer to the function's generic model entry
   * within the exception table, which starts with the personality routine's
   * address.
   * @return std::uint32_t const* - pointer to the LSDA
   */
  [[gnu::always_inline]] [[nodiscard]] static constexpr std::uint32_t const*
  lsda_data(std::uint32_t const* p_personality)
  {
    constexpr auto generic_count = hal::bit_mask::from<24, 31>();
    auto const* header = p_personality + 1;
    return (header + 1) + hal::bit_extract<generic_count>(*header);
  }

  /**
   * @brief Returns the pointer to the personality's descriptor data
   *
   * Descriptor data is data that comes after the unwind instructions.
   *
   * @param p_personality - pointer to the function's personality data within
   * the exception table.
   * @return std::uint32_t const* - pointer to the descriptor start area. Always
   * valid so long as p_personality is valid.
   */
  [[gnu::always_inline]] static constexpr std::uint32_t const* descriptor_start(
    std::uint32_t const* p_personality)
  {
    // A compact model entry starts with its unwind instructions. A generic
    // model entry starts with the personality routine's address, and its
    // unwind instructions follow it.
    auto const* header = p_personality;
    if (not is_compact_model(*header)) {
      header++;
    }
    // Fast path for nearly every function: compact personality routine 0, or
    // the generic model with no extra words.
    constexpr std::uint32_t top_byte_without_model_bit = 0x7f00'0000;
    if ((*header & top_byte_without_model_bit) == 0) {
      return header + 1;
    }
    return (header + 1) + unwind_word_count(*header);
  }

  [[gnu::always_inline]] [[nodiscard]] function_t function() const
  {
    return { to_absolute_address(&function_offset) };
  }
};

struct cortex_m_cpu
{
  register_t r0;
  register_t r1;
  register_t r2;
  register_t r3;
  register_t r4;
  register_t r5;
  register_t r6;
  register_t r7;
  register_t r8;
  register_t r9;
  register_t r10;
  register_t r11;
  register_t ip;
  register_t sp;
  register_t lr;
  register_t pc;

  register_t& operator[](size_t p_size)
  {
    using register_file = std::array<register_t, 16>;
    static_assert(sizeof(cortex_m_cpu) == sizeof(register_file));
    static_assert(sizeof(std::uintptr_t) == sizeof(register_t));

    auto* file = reinterpret_cast<register_file*>(this);
    return (*file)[p_size];
  }
};

enum class runtime_state : std::uint8_t
{
  handled_state = 0,
  get_next_frame = 1,
  enter_function = 2,
  unwind_frame = 3,
};

struct exception_cache
{
  index_entry_t const* entry_ptr = nullptr;
  std::uint32_t const* personality = nullptr;
  std::uint32_t rel_address;
  runtime_state inner_state;
  bool previously_rethrown = false;

  static constexpr auto state_mask = hal::bit_mask{
    .position = 24,
    .width = 4,
  };
  static constexpr auto rethrown_mask = hal::bit_mask{
    .position = 28,
    .width = 1,
  };
  static constexpr auto relative_address_mask = hal::bit_mask{
    .position = 0,
    .width = 24,
  };

  [[nodiscard]] constexpr runtime_state state() const
  {
    return inner_state;
  }

  [[nodiscard]] constexpr std::uint32_t relative_address() const
  {
    return rel_address;
  }

  constexpr void state(runtime_state p_state)
  {
    inner_state = p_state;
  }

  constexpr void rethrown(bool p_rethrown)
  {
    previously_rethrown = p_rethrown;
  }

  [[nodiscard]] constexpr bool rethrown() const
  {
    return previously_rethrown;
  }

  constexpr void relative_address(std::uint32_t p_rel_address)
  {
    rel_address = p_rel_address;
  }
};

struct base_class_type_info
{
  void const* type_info = nullptr;
  std::ptrdiff_t offset = 0;
};

template<std::size_t max_count>
struct flattened_hierarchy
{
  static_assert(max_count > 0);
  static constexpr auto capacity = max_count;

  std::array<base_class_type_info, max_count> bases{};
  std::uint32_t m_size = 0;

  explicit constexpr flattened_hierarchy(std::type_info const* p_info)
  {
    bases[0].type_info = p_info;
    bases[0].offset = 0;
    m_size++;
  }

  flattened_hierarchy() = default;

  [[nodiscard]] constexpr auto begin()
  {
    return bases.begin();
  }

  [[nodiscard]] constexpr auto end()
  {
    return bases.begin() + m_size;
  }

  [[nodiscard]] constexpr auto cbegin() const
  {
    return bases.cbegin();
  }

  [[nodiscard]] constexpr auto cend() const
  {
    return bases.cbegin() + m_size;
  }

  constexpr void push_back(base_class_type_info const& p_info)
  {
    if (m_size > max_count) {
      std::terminate();
    }
    bases[m_size++] = p_info;
  }

  using iter = decltype(bases.end());

  constexpr iter find(std::type_info const* p_type)
  {
    for (auto const base : bases) {
      if (base->type_info == p_type) {
        return &base;
      }
    }
    return bases.end();
  }

  constexpr auto size()
  {
    return m_size;
  }

  constexpr void reset()
  {
    m_size = 0;
  }
};

template<typename T = std::byte>
struct exception_allocation
{
  std::size_t size = 0uz;
  alignas(std::max_align_t) T data;
};

template<typename T = std::byte>
exception_allocation<T>* get_allocation_from_exception(void* p_exception_ptr)
{
  auto const member_offset = offsetof(exception_allocation<T>, data);
  auto const exception_bytes = static_cast<std::byte*>(p_exception_ptr);
  return reinterpret_cast<exception_allocation<T>*>(exception_bytes -
                                                    member_offset);
}

struct exception_control_block
{
  cortex_m_cpu cpu{};
  flattened_hierarchy<4> type_info{};
  void* thrown_object;
  std::size_t choosen_type_offset = 0;
  destructor_t* destructor = nullptr;
  exception_cache cache{};
};

constexpr auto ecb_size = sizeof(exception_control_block);

struct [[gnu::packed]] su16_t
{
  std::uint8_t instruction0 : 8;
  std::uint8_t instruction1 : 8;
  std::uint8_t instruction2 : 8;
  std::uint8_t type : 4;
  std::uint8_t reserved : 3;
  std::uint8_t handler_data_flag : 1;
};

struct [[gnu::packed]] lu_t
{
  std::uint8_t instruction0 : 8;
  std::uint8_t instruction1 : 8;
  std::uint8_t length : 8;
  std::uint8_t type : 4;
  std::uint8_t reserved : 3;
  std::uint8_t handler_data_flag : 1;
};
}  // namespace except

// NOLINTBEGIN(bugprone-reserved-identifier)
// NOLINTBEGIN(readability-identifier-naming)
extern "C"
{
  extern except::index_entry_t const __exidx_start;
  extern except::index_entry_t const __exidx_end;
}
// NOLINTEND(readability-identifier-naming)
// NOLINTEND(bugprone-reserved-identifier)
