/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>
#include "errno.h"

/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %edx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %ebx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

// Para avisar al sistema de que ya hemos recibido la interrupción y que 
// ya podemos recibir nuevas interrupciones.
// NOTA: Para no complicar la implementación, este sistema operativo no acepta nuevas
//       interrupciones hasta acabar el tratamiento de la actual. Sin embargo, los SO's 
//       sí que lo permiten.
#define EOI \
      movb $0x20, %al ; \
      outb %al, $0x20 ; 

// Guarda el contexto en la pila
#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %ebx; \
      pushl %ecx; \
      pushl %edx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

// Restaura el contexto de la pila
#define RESTORE_ALL \
      popl %edx; \
      popl %ecx; \
      popl %ebx; \
      popl %esi; \
      popl %edi; \
      popl %ebp; \
      popl %eax; \
      popl %ds; \
      popl %es; \
      popl %fs; \
      popl %gs 


ENTRY(clock_handler) 
      #El procesador guarda el context HW automáticamente

      # guardamos el context SW
      SAVE_ALL

      # end of interruption
      EOI

      # syscall a clock_routine
      call clock_routine

      # restauramos el context SW
      RESTORE_ALL

      # pasamos de privilege mode -> user mode
      iret  

ENTRY(keyboard_handler)
	SAVE_ALL
	call keyboard_routine
      EOI
	RESTORE_ALL
	iret

ENTRY(page_fault__handler) 
      # no hacemos un SAVE ALL porque nos quedamos en un infinite loop
      
      #syscall 
      call page_fault__routine

      #infinite loop...


ENTRY(syscall_handler_sysenter)
      push $__USER_DS
      push %EBP // User stack address
      pushfl
      push $__USER_CS
      push 4(%EBP) // User return address
      SAVE_ALL

      cmpl $0, %EAX   // is %eax < 0 ?
      jl sysenter_err // if yes then go to sysenter_err

      cmpl $MAX_SYSCALL, %EAX // is %eax > $MAX_SYSCALL?
      jg sysenter_err // if yes then go to sysenter_err

      call *sys_call_table(, %EAX, 0x04)
      jmp sysenter_fin

      sysenter_err:

      movl $-ENOSYS, %EAX
      sysenter_fin:
      movl %EAX, 0x18(%ESP)
      RESTORE_ALL
      movl (%ESP), %EDX // Return address
      movl 12(%ESP), %ECX // User stack address
      sti // Enable interrupts again
      sysexit


ENTRY(writeMSR)
	push %ebp
      movl %esp,%ebp

      # NOTA: Usamos estos registros porque la propia MSR lo dicta
      movl 8(%ebp), %ecx # 1r parám. = número de MSR
      movl 12(%ebp),%eax # 2o parám. = parte baja
      movl $0,%edx # 2o. parám = la parte alta a 0, zeos usa 32 bits

      wrmsr 

      popl %ebp
      ret

