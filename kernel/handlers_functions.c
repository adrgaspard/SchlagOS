#include <stdint.h>

#include "../shared/consts.h"
#include "handlers_functions.h"
#include "segment.h"
#include "cpu.h"
#include "../shared/debug.h"

#define HANDLER_MAP_ADDRESS 0x1000

/**
 * @brief Initialise un traitant d'interruption dans la table des traitants d'interruption
 * 
 * @param N Index du traitant d'interruption
 * @param handler Pointeur vers la fonction qui traite l'interruption
 * @param options Vecteur d'options associée au traitant d'interruption
 */
static void handler_init(int N, void (* handler)(void), uint32_t options){
    uint32_t * base_addr = (uint32_t *) HANDLER_MAP_ADDRESS;
    base_addr += 2*N; //Words are stored by two

    *base_addr = KERNEL_CS << 16 | (((uint32_t) handler) & 0x0000FFFF); // 1
    base_addr++; // We prepare to write second word
    *base_addr = (((uint32_t) handler) & 0xFFFF0000) | (options & 0x0000FFFF);
}

void mask_IRQ_channel(int N){
    uint8_t curr_val = inb(IRQ_0_7_VECTOR_PORT);
    outb(curr_val | 1<<N, IRQ_0_7_VECTOR_PORT); // We force the Nth bit to be 1, IRQ(N) is masked
}

void unmask_IRQ_channel(int N){
    uint8_t curr_val = inb(IRQ_0_7_VECTOR_PORT);
    outb(curr_val & ~(1<<N), IRQ_0_7_VECTOR_PORT); // We force the Nth bit to be 0, IRQ(N) is unmasked
}

extern void handler_IT_32(void);
extern void handler_IT_33(void);
extern void handler_IT_49(void);

void handlers_init(){
    handler_init(32, handler_IT_32, IT_VECTOR_P_VALID | IT_VECTOR_TYPE_INTERRUPT);
    handler_init(33, handler_IT_33, IT_VECTOR_P_VALID | IT_VECTOR_TYPE_INTERRUPT);
    handler_init(49, handler_IT_49, IT_VECTOR_P_VALID | IT_VECTOR_DPL_USERMODE | IT_VECTOR_TYPE_INTERRUPT);
}