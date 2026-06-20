#ifndef MM_ADDRESS_H
#define MM_ADDRESS_H

#define PAGE_TABLE_ENTRIES 1024 // entradas que tiene una tp
#define TOTAL_PAGES 2048    //total de paginas fisicas que tenemos de memoria
#define PAGE_SIZE 0x1000 // pagina tiene tamaño de 0x1000 = 4096


//                                                                          USER**
#define PAG_LOG_INIT_CODE (PAG_LOG_INIT_DATA+NUM_PAG_DATA)
#define PAG_LOG_INIT_DATA (L_USER_START>>12)
/*
    Antes L_USER_START =  0x400000, entonces PAG_LOG_INIT_DATA = frame 1024 = inicio dir[1]
    AHORA L_USER_START = 0x800000, entonces el PAG_LOG_INIT_DATA = 2048 =  inicio dir[2]
    los frames de usuario ahora empiezan en el frame logico 2048 ahora
*/


#define NUM_PAG_DATA 20 //frames que usa el user  para datos + pila
#define NUM_PAG_CODE 8  //frames que usa el user  para cdogio 

/* Memory distribution */
/***********************/
/*
Dir[0] -> 0x00000000 - 0x003FFFFF   kernel físico bajo
Dir[1] -> 0x00400000 - 0x007FFFFF   kernel físico alto
Dir[2] -> 0x00800000 - 0x00BFFFFF   usuario
*/

#define KERNEL_START     0x10000
#define L_USER_START        0x800000
#define USER_ESP	L_USER_START+(NUM_PAG_DATA)*0x1000

#define PAGE(x) (x>>12)

#endif


/* L_USER_START = 0x800000 -> pág. lóg 2048
 * -------------------  <- 0x800000 (Empiezan los DATOS)
 * |      DATA        |
 * |                  |  
 * |                  |  (NUM_PAG_DATA = 20 págs. lóg = [2048,2067])
 * |        ^         |
 * |        |         |
 * |    USER STACK    |
 * -------------------  <- 0x814000 (USER_ESP) = inicio de la pila
 * |                  |
 * |     CÓDIGO       |  (NUM_PAGE_CODE = 8 págs. lógs = [2068,2075])
 * |                  |
 * -------------------
 * 
 * **/
