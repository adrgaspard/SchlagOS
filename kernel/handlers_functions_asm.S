#include "../shared/consts.h"

    .text 

# cette directive sert a rendre l'etiquette publique
    .globl handler_IT_32
# debut du traitant 
handler_IT_32:
# sauvegarde des registres importants
    pushl %eax
    pushl %edx
    pushl %ecx

    # On passe en mode superviseur
    mov $DATA_SEGMENT_KERNEL_VALUE, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

# appel a la fonction C realisant le traitant
    call tic_PIT

    mov $DATA_SEGMENT_USER_VALUE, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
# restauration des registres importants
    popl %ecx
    popl %edx
    popl %eax
# fin du traitant
    iret

    .globl handler_IT_33
handler_IT_33:
    # sauvegarde des registres importants
    pushl %eax
    pushl %edx
    pushl %ecx

    # On passe en mode superviseur
    mov $DATA_SEGMENT_KERNEL_VALUE, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

# appel a la fonction C realisant le traitant
    call keyboard_interrupt

    mov $DATA_SEGMENT_USER_VALUE, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
# restauration des registres importants
    popl %ecx
    popl %edx
    popl %eax
# fin du traitant
    iret

# eax, ebx, ecx, edx, esi, edi
    .globl handler_IT_49
handler_IT_49:
    # Passage des args en ordre inverse
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    # On passe en mode superviseur
    mov $DATA_SEGMENT_KERNEL_VALUE, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    # On appelle le traitant d'interruption
    call syscall_handler
    add $4, %esp # On oublie eax
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi

    pushl %eax
    # On repasse en mode utilisateur
    mov $DATA_SEGMENT_USER_VALUE, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    popl %eax
    iret
