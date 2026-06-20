/*
 * hardware.h - Rutines hardware per manegar els accesos a baix nivell
 */

#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#include <types.h>

DWord get_eflags(void);
void set_eflags(void);
void set_idt_reg(Register * idt);
void set_gdt_reg(Register * gdt);
void set_ldt_reg(Selector ldt);
void set_task_reg(Selector tr);
void return_gate(Word ds, Word ss, DWord esp, Word cs, DWord eip);

/*
 * enable_int: Assigna la mascara d'interrupcions
 *
 *    registro 0x21:
 *    7 6 5 4 3 2 1 0
 *    x x x x x x x x
 *    
 *    bit 0 : Timer
 *    bit 1 : Keyboard
 *    bit 2 : PIC cascading
 *    bit 3 : 2nd Serial Port
 *    bit 4 : 1st Serial Port
 *    bit 5 : Reserved
 *    bit 6 : Floppy disk
 *    bit 7 : Reserved
 * 
 *
 *   x = 0 -> activada
 *   x = 1 -> no activada
 */

void enable_int(void);
void delay(void);

Byte inb (unsigned short port);

void bochs_out(char c);

/* Writes on CR3 register producing a TLB flush */
void set_cr3(page_table_entry * dir);

int read_cr0();

void write_cr0(int value);

void __sti(void);

struct task_struct * current();

/**************************
 ** setSegmentRegisters ***
 **************************
 * Set properly all the registers, used
 * at initialization code.
 *   DS, ES, FS, GS <- DS
 *   SS:ESP <- DS:DATA_SEGMENT_SIZE
 *         (the stacks grows towards 0)
 *
 * cld -> gcc2 wants DF (Direction Flag (eFlags.df))
 *        always clear.
 */
void set_seg_regs(Word data_sel, Word stack_sel, DWord esp);

#endif  /* __HARDWARE_H__ */
