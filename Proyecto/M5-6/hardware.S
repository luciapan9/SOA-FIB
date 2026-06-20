#include <asm.h>

// Returns EFLAGS
ENTRY(get_eflags)
    pushf
    popl %eax
    ret

// Set EFLAGS
// void set_eflags(void)
ENTRY(set_eflags)
    pushl $0
    popfl
    ret

// Set IDT register
// void set_idt_reg(Register * idt)
ENTRY(set_idt_reg)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    lidtl (%eax)
    popl %ebp
    ret

// Set GDT register
// void set_gdt_reg(Register *gdt)
ENTRY(set_gdt_reg)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    lgdtl (%eax)
    popl %ebp
    ret

// Set ldt selector
// void set_ldt_reg(Selector ldt)
ENTRY(set_ldt_reg)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    lldtw %ax
    popl %ebp
    ret

// Set tss register
// void set_task_reg(Selector tr)
ENTRY(set_task_reg)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    ltrw %ax
    popl %ebp
    ret

// Return gate - jmp to userland
// void return_gate(word ds, word ss, dword esp, word cs, dword eip)
ENTRY(return_gate)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movw %ax, %es
    movw %ax, %ds
    movl 16(%ebp), %eax
    subl $16, %eax
    movl (p_rdtr), %ecx
    movl (%ecx), %ecx
    movl %ecx, 12(%eax)
    pushl 12(%ebp)
    pushl %eax
    pushl 20(%ebp)
    pushl 24(%ebp)
    lret

/*
 * enable_int: Set interruput mask
 *
 *    register 0x21:
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
 *   x = 0 -> enabled
 *   x = 1 -> disabled
 */
// void enable_int(void)
ENTRY(enable_int)
    pushl %ebp
    movl %esp, %ebp
    movb $0xfc, %al /* 0xFC = 11111100 -> all bits disabled except 
                        the clock and the keyboard bits */
    outb %al, $0x21
    call delay
    sti
    pop %ebp
    ret
delay:
    jmp a
a:
    ret

// Reads a byte from a Port
// Byte inb (unsigned short port)
ENTRY(inb)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %edx
    inb (%dx), %al
    popl %ebp
    ret

// Magic Bochs output
// void bochs_out(char c)
ENTRY (bochs_out)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    outb %al, $0xe9
    popl %ebp
    ret

// Sets cr3 register
// void set_cr3(page_table_entry * dir)
ENTRY(set_cr3)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl %eax, %cr3
    popl %ebp
    ret

// Reads cr0 register
// int read_cr0()
ENTRY(read_cr0)
    movl %cr0, %eax
    ret

// Sets cr0 register
// void write_cr0(int value)
ENTRY(write_cr0)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl %eax, %cr0
    popl %ebp
    ret

// Intrinsic for sti
// void __sti(void)
ENTRY(__sti)
    sti
    ret

// Calculates the address of current task_union
// union task_union *current()
ENTRY(current)
    movl %esp, %eax
    andl $0xFFFFF000, %eax
    ret

// Set selector registers and stack
// void set_seg_regs(Word data_sel, Word stack_sel, DWord esp)
ENTRY(set_seg_regs)
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    cld
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movl 12(%ebp), %eax
    movl 16(%ebp), %ecx
    subl $0x10, %ecx // Save some space in case caller tries to "clean up the parameters"
    movl 4(%ebp), %edx // saves the return address
    movw %ax, %ss
    movl %ecx, %esp
    pushl %edx
    ret