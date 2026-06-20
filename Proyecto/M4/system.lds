/*
 *  ZeOS
 *  system.lds - Linker Script for ZeOS system image.
 */

ENTRY(main)
SECTIONS
{
  . = 0x10000;
    
  .text.main : { 
    /* reserved to store user code size */
    . += 16;
    *(.text.main) 
  }
                                     
  .text : { *(.text) }
  .rodata : { *(.rodata) }
  .data : { *(.data) }
  .bss : { *(.bss) }

  . = ALIGN(4096);              /* Magical section that marks the final size of the system data */
  .data.last : { *(.data.last) }
}
