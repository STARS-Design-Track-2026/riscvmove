/* link.ld */
OUTPUT_ARCH( "riscv" )
ENTRY(_start)

MEMORY
{
  /* Matches src/imem.sv's DEPTH parameter (default 512 words x 32 bits) --
     keep this in sync if that default ever changes. */
  IMEM (rx) : ORIGIN = 0x00000000, LENGTH = 2K
  /* Matches src/dmem.sv's DEPTH parameter (default 256 words x 32 bits) --
     keep this in sync if that default ever changes. ORIGIN is nonzero only
     so IMEM/DMEM don't collide in ld's flat LMA address space -- dmem.sv
     itself only ever looks at the low address bits (see addr_b indexing),
     so the hardware doesn't care what this base is. */
  DMEM (rw) : ORIGIN = 0x10000000, LENGTH = 1024
}

SECTIONS
{
  .text : {
    *(.text.init)
    *(.text .text.*)
  } > IMEM

  /* .rodata lives in dmem, not imem, even though software never writes it --
     there's no lw/lb/lh path from the data port back into imem's BRAM
     (imem's only port is the pc-driven fetch port), so any runtime load of
     a string literal or const array (e.g. uart_putstr("...")) would
     otherwise read whatever garbage is at that address in dmem instead of
     the real data. Same underlying constraint as .data below, and the same
     fix: dmem.sv's own INIT_FILE preloads this section's contents (see the
     Makefile: it objcopy's .rodata and .data together into dmem's hex). */
  .rodata : {
    *(.rodata .rodata.*)
  } > DMEM

  /* .data lives entirely in dmem -- there's no lw/sw path from dmem back
     into imem's BRAM, so unlike a typical Harvard-style split, initialized
     globals can't be copied in from a load image at boot. Instead
     dmem.sv's own INIT_FILE preloads this section's initial values
     directly (see the Makefile: it objcopy's *just* .rodata and .data out
     of the ELF into dmem's own hex file for that parameter). */
  .data : {
    *(.data .data.*)
    *(.sdata .sdata.*)
  } > DMEM

  .bss : {
    _bss_start = .;
    *(.bss .bss.*)
    *(.sbss .sbss.*)
    *(COMMON)
    _bss_end = .;
  } > DMEM

  _dmem_top = ORIGIN(DMEM) + LENGTH(DMEM);
  _end = .;
}
