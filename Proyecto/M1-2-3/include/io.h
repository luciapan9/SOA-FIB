/*
 * io.h - Definició de l'entrada/sortida per pantalla en mode sistema
 */

#ifndef __IO_H__
#define __IO_H__

#include <types.h>

/** Screen functions **/
/**********************/

void printc(char c);
void printc_xy(Byte x, Byte y, char c);
void printk(char *string);
void set_cursor_pos(Byte mx, Byte my);
void set_color_attr(Byte fg, Byte bg);

#endif  /* __IO_H__ */
