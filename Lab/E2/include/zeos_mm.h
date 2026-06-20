#include <mm.h>

char* itoa_hex(int a, char *b) ;


// Prepare address space for initial process
void prepare_mono_address_space();

/* Print page table translation for a PT from start to start + numpages, adding a msg at the end */
void show_PT_range(page_table_entry* PT, int start, int numpages, char* msg) ;
void show_PT(page_table_entry* PT);
