/*
 * Copyright 2026 Khalil Estell and the libhal contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Memory map for the tests that run under QEMU. QEMU's netduino2 (STM32F205,
 * Cortex-M3) and netduinoplus2 (STM32F405, Cortex-M4F) both have 1 MB of flash
 * at 0x08000000 and at least 128 kB of RAM at 0x20000000.
 */
MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

ENTRY(reset_handler)

SECTIONS
{
  .text :
  {
    KEEP(*(.vectors))
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
  } > FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH

  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } > FLASH

  .init_array :
  {
    __init_array_start = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } > FLASH

  __data_load = LOADADDR(.data);
  .data :
  {
    __data_start__ = .;
    *(.data*)
    . = ALIGN(4);
    __data_end__ = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    __bss_start__ = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAM

  .tbss (NOLOAD) : { *(.tbss*) } > RAM

  end = .;
  _end = .;
  __end__ = .;
  __stack_top = ORIGIN(RAM) + LENGTH(RAM);

  /* picolibc's malloc() uses the RAM between the data and the stack, which
     gets the top 8 kB. */
  __heap_start = .;
  __heap_end = __stack_top - 8K;
}
