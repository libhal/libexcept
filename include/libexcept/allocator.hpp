#pragma once

#include <cstddef>

#include <new>

namespace except {
struct allocation_tag
{};
}  // namespace except

void* operator new(std::size_t p_size,
                   std::align_val_t p_align,
                   except::allocation_tag);

void operator delete(void* p_ptr,
                     std::size_t p_size,
                     std::align_val_t p_align,
                     except::allocation_tag);
