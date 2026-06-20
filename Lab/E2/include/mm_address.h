#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#define TOTAL_PAGES 1024
#define PAG_LOG_INIT_CODE (PAG_LOG_INIT_DATA+NUM_PAG_DATA)
#define NUM_PAG_CODE 8
#define PAG_LOG_INIT_DATA (L_USER_START>>12)
#define NUM_PAG_DATA 20
#define PAGE_SIZE 0x1000

/* Memory distribution */
/***********************/


#define KERNEL_START     0x10000
#define L_USER_START        0x400000
#define USER_ESP	L_USER_START+(NUM_PAG_DATA)*0x1000

#define PAGE(x) (x>>12)

#endif

/* L_USER_START = 0x400000 -> pág. lóg 1024
 * -------------------  <- 0x400000 (Empiezan los DATOS)
 * |      DATA        |
 * |                  |  
 * |                  |  (NUM_PAG_DATA = 20 págs. lóg = [1024,1043])
 * |        ^         |
 * |        |         |
 * |    USER STACK    |
 * -------------------  <- 0x414000 (USER_ESP) = inicio de la pila
 * |                  |
 * |     CÓDIGO       |  (NUM_PAGE_CODE = 8 págs. lógs = [1044,1051])
 * |                  |
 * -------------------
 * 
 * **/