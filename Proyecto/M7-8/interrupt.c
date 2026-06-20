/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <sched.h>
#include <io.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

// asm
void keyboard_handler(void);
void clock_handler(void);
void page_fault__handler(unsigned long error,unsigned long eip);
int writeMSR(unsigned long dir,int value);
void syscall_handler_sysenter(void);


int zeos_ticks = 0;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}


void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;

  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(14,page_fault__handler,0);  // page fault handler
  setInterruptHandler(32,clock_handler,0);  // clock handler
  setInterruptHandler(33,keyboard_handler,0); //keyboard handler


  set_idt_reg(&idtR);


  writeMSR(0x174,__KERNEL_CS);
  writeMSR(0x175,INITIAL_ESP);
  writeMSR(0x176,(int)syscall_handler_sysenter);

}


/** @brief Keyboard Interrupt Service Routine (ISR)
 *  Atiende la interrupción de teclado, traduce el scan code y lo muestra por pantalla
 *  1) Obtiene el byte del Keyboard Data Register (puerto 0x60)
 *     @see unsigned char inb() from hardware.S
 *  2) Evalua el bit 7 para distinguir si se trata de un make(bit == 1) o un break(bit ==0)
 *  3) Si es un make(tecla pulsada), coge los bits 0..6 correspondientes al scan_code para
 *     traducirlo a caràcter
 *  4) Si el carácter no es de tipo ASCII(espacio,Control,...) se le asigna una 'C,
 *     sino, se le asigna su valor ASCII correspondiente.
 *  5) Imprime por pantalla en la esquina superior izquierda el carácter.
 *     @see printc_xy(Byte x,Byte y,char c) from io.c
 */
void keyboard_routine(void){
  unsigned char byte = inb(0x60);       
  if (byte & 0x80) return;

  unsigned int scan_code = byte & 0x7F;
  if (scan_code >= sizeof(char_map)) return;

  unsigned char output = (char_map[scan_code] == '\0' ? 'C' : char_map[scan_code]);

  keyboard_store_char(output);  //Guardamos la tecla pulsada en el buffer circular, la dejamo aqui para luego poder consultarla con read
}

/** @brief Page Fault exception
 *  Atiende la excepción de fallo de página parando el sistema por completo.
 * 
 *  Si pasamos el error code i el eip por parámetro, estos coinciden con la pila
 *  |   ebp   | <--- esp
 *  |   @ret  |
 *  |  error  |
 *  |   eip   |
 *  Usando este "truco", podemos acceder a sus valores.
 * 
 *  Uso de la función:
 *  1) Imprime por pantalla que ha habido una page fault exception en la direción
 *     designada por eip
 *  2) Al ser una excepción, el sistema se queda parado ejecutando un bucle infinito.
 * 
 *  Parámetros de la función:
 *  @param error : contiene la dirección del error
 *  @param eip   : contiene la dirección de la instrucción que ha causado la excepción
 */
void page_fault__routine(unsigned long error,unsigned long eip) {
  printk("\nProcess generates a PAGE FAULT exception at EIP: 0x");

  //eip contiene la @ de memoria de tipo int, lo pasamos a char
  const char HEX[16] = "0123456789ABCDEF";
  char dir[9]; // 9 = 8(32/4)b + 1 (\'0') 

  int num;
  for (int i = 7; i >= 0; --i) {
    num = (eip >> (4*i)) & 0x0F;
		dir[7-i] = HEX[num];
  }
  dir[8] = '\0';

  printk(dir);
  printk("\ninfinite loop... :/ \n");
  while(1);
}

/** @brief Clock Interrupt Service Routine(ISR)
 *  Llama a la función zeos_show_clock() ya implementada.
 *  @note el reloj va más deprisa 
 */
void clock_routine(void) {
  //zeos_show_clock();
  zeos_ticks++;

  schedule();
}

