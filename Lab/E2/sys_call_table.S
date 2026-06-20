#include <asm.h>
#include <segment.h>

ENTRY (sys_call_table)
	.long sys_ni_syscall // 0
	.long sys_exit // 1 = sys_exit
	.long sys_fork // 2 = sys_fork
	.long sys_ni_syscall // 3
	.long sys_write // 4 = sys_write
	.long sys_block // 5 = block
	.long sys_unblock // 6 = unblock 
	.long sys_ni_syscall // 7
	.long sys_ni_syscall // 8
	.long sys_ni_syscall // 9
	.long sys_gettime // 10 = sys_gettime
	.long sys_ni_syscall // 11
	.long sys_ni_syscall // 12
	.long sys_ni_syscall // 13
	.long sys_ni_syscall // 14
	.long sys_ni_syscall // 15
	.long sys_ni_syscall // 16
	.long sys_ni_syscall // 17
	.long sys_ni_syscall // 18
	.long sys_ni_syscall // 19
	.long sys_getpid // 20 = sys_getpid
	.globl MAX_SYSCALL
	MAX_SYSCALL = (. - sys_call_table)/4