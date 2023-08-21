    .globl exit_handler
exit_handler:
    pushl %eax
    call exit
    ret # exit est sans retour, donc cette ligne ne sera jamais appelée 
