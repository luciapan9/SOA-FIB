#include <asm.h>


# eax, ecx, edx   -> se dan por hecho que se modifican, no hay que salvarlos
# edx, esi, edi, ebp -> si una función los utiliza hay que guardarlos

ENTRY(exit)
    # 1. dynamic link 
    pushl %ebp 
    movl %esp,%ebp 

    # 2. rutina de servicio 
    movl $1,%eax

    # 3. registros necesarios

    # 4. paso de parámetros 

    # 5. fake dynamic link
    pushl $exit_return 
    pushl %ebp 
    movl %esp,%ebp 

    # 6. sysenter
    sysenter

exit_return:
    # 7.1 limpiamos pila 
    popl %ebp 
    addl $4,%esp
    # 7.2 restauramos registros

    # 8. procesamos resultado
    cmpl $0,%eax  # is [%eax] < 0 ?
    jl exit_error # if yes then go to exit_error
    jmp end_exit  # else jump end 

exit_error:
    negl %eax        # ponemos en positivo el valor de errno 
    movl %eax,errno  # errno tiene el error code 
    movl $-1,%eax    # al user le devolvemos -1 para indicarle error

end_exit:
    # 9. return 
    popl %ebp 
    ret


ENTRY(getpid)
    # 1. dynamic link
    pushl %ebp
    movl %esp, %ebp

    # 2. rutina de servicio en %eax 
    movl $20,%eax

    # 3. guardamos los registros necesarios
    pushl %ebx 

    #5. fake dynamic link 
    pushl $getpid_return # store de la @ de retorno al wrapper
    pushl %ebp # fake dynamic link
    movl %esp,%ebp 

    # 6. fast syscall
    sysenter

getpid_return:
    # 7.1 Limpiamos pila 
    popl %ebp
    addl $4,%esp
    # 7.2 Restauramos registros
    popl %ebx

    # 8. Procesamos el resultado
    cmpl $0,%eax  # is ([%eax] < 0) ?
    jl getpid_error  # if yes then go to getpid_error
    jmp end_getpid   # else jump end 

getpid_error:
    negl %eax # en positivo el valor del errno
    movl %eax,errno # errno tiene el error code 
    movl $-1,%eax # al usuario le devolvemos -1

end_getpid:
    # 9. Return
    popl %ebp 
    ret 



ENTRY(fork)
    # 1. dynamic link 
    pushl %ebp 
    movl %esp,%ebp 

    # 2. rutina de servicio
    movl $2,%eax

    # 3. guardamos registros
    #pushl %esi 
    #pushl %edi 
    pushl %ebx

    # 5. fake dynamic link 
    pushl $fork_return
    pushl %ebp 
    movl %esp,%ebp 

    # 6. fast syscall
    sysenter

fork_return:
    # 7. undo dynamic link 
    popl %ebp
    addl $4,%esp 
    popl %ebx 

    # 8. Procesamos el resultado
    cmpl $0,%eax  
    jl fork_error  
    jmp end_fork  

fork_error:
    negl %eax # en positivo el valor del errno
    movl %eax,errno # errno tiene el error code 
    movl $-1,%eax # al usuario le devolvemos -1

end_fork:
    # 9. Return
    popl %ebp 
    ret 


ENTRY(write)
    # 1. dynamic link
    pushl %ebp
    movl %esp,%ebp

    # 2. ponemos el id del sys_write en %eax
    movl $4,%eax

    # 3. Guardamos los registros necesarios 
    pushl %ebx 

    # 4. Paso de parámetros: int write(int fd,char* buffer,int size)
    movl 8(%ebp),%edx  # %edx <- fd  (1r parám.)
    movl 12(%ebp),%ecx # %ecx <- buffer (2o parám.)
    movl 16(%ebp),%ebx # %ebx <- size  (3r parám.)

    # 5. Fake dynamic link 
    /** una vez termina se termina el tratamiento de la **/
    /** syscall, con el sysexit el kernel no sabe como  **/
    /** volver a la @ de vuelta del wrapper. Para ello, **/
    /** creamos un fake dynamic link -> guardamos la    **/
    /** dirección de retorno al wrapper.                **/

    pushl $write_return # store de la @ de retorno al wrapper
    pushl %ebp # fake dynamic link
    movl %esp,%ebp 

    /** Pila actual **/
    # |   ebp   | 
    # |  @ret   | <- $write_return, return de la siguiente inst. del wrapper.
    # |   edx   |                           
    # |   ecx   |
    # |   edx   |
    # | old_ebp |
    # |  @ret   | <- return a la siguiente instrucción de usuario

    # 6. Fast system call
    sysenter 

write_return:
    # 7.1 Limpiamos pila 
    popl %ebp
    addl $4,%esp
    # 7.2 Restauramos registros
    popl %ebx

    # 8. Procesamos el resultado
    cmpl $0,%eax  # is ([%eax] < 0) ?
    jl write_error  # if yes then go to write_error
    jmp end_write   # else jump end_write 

write_error:
    negl %eax # en positivo el valor del errno
    movl %eax,errno # errno tiene el error code 
    movl $-1,%eax # al usuario le devolvemos -1

end_write:
    # 9. Return
    popl %ebp 
    ret 


ENTRY(gettime)
    # link dinamico
    push %ebp
    movl %esp, %ebp

    # ponemos el número de la syscall en eax
    movl $10, %eax

    # metemos la dir de retorno y creamos el fake dynamic link
    push $return_gettime
    pushl %ebp # fake dynamic link
    movl %esp,%ebp

    sysenter

return_gettime:
    # restauramos ebp y quitamos el @return_gettime de la pila
    popl %ebp
    addl $4, %esp

    # miramos si gettime nos ha devuelto error
    cmpl $0, %eax  
    jl gettime_error 

    # si no nos ha dado error vamos directamente al fin
    jmp fin 

gettime_error:
    negl %eax # en positivo el valor del errno
    movl %eax,errno # errno tiene el error code 
    movl $-1,%eax # al usuario le devolvemos -1

fin:
    # deshacemos dynamic link
    popl %ebp 
    ret 


ENTRY(block)
    pushl %ebp
    movl %esp, %ebp

    movl $5, %eax  

    pushl $block_return
    pushl %ebp
    movl %esp, %ebp

    sysenter

block_return:
    popl %ebp
    addl $4, %esp

    cmpl $0, %eax
    jl block_error
    jmp end_block

block_error:
    negl %eax
    movl %eax, errno
    movl $-1, %eax

end_block:
    popl %ebp
    ret


ENTRY(unblock)
    pushl %ebp
    movl %esp, %ebp

    movl $6, %eax   

    movl 8(%ebp), %edx  

    pushl $unblock_return
    pushl %ebp
    movl %esp, %ebp

    sysenter

unblock_return:
    popl %ebp
    addl $4, %esp

    cmpl $0, %eax
    jl unblock_error
    jmp end_unblock

unblock_error:
    negl %eax
    movl %eax, errno
    movl $-1, %eax

end_unblock:
    popl %ebp
    ret
    

