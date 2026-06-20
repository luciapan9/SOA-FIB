#ifndef DEVICES_H__
#define  DEVICES_H__

int sys_write_console(char *buffer,int size);
void init_keyboard_buffer(void);
void keyboard_store_char(char c);
int keyboard_buffer_empty(void);
int keyboard_read_char(char *c);
int keyboard_can_current_read(void);
void keyboard_finish_current_read(void);
void keyboard_block_current(void);
#endif /* DEVICES_H__*/
