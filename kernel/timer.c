#include <stdint.h>

#include "../shared/console.h"
#include "segment.h"
#include "cpu.h"
#include "debug.h"
#include "timer.h"
#include "process.h"
#include "../shared/consts.h"

uint32_t inter = 0, sec = 0, min = 0, hour = 0, frac_sec = 0, scheduler_delay = 0;

/**
 * @brief  Cette fonction est appelée à chaque interruption d'horloge par le traitant de l'interruption 32.
 *          Elle gère:
 *            - L'affichage du temps passé depuis le lancement du système au format HH:MM:SS
 *            - L'appel de l'ordonnanceur à la fréquence spécifiée dans les constantes système
 * 
 */
void tic_PIT(void){
    // On prévient le contrôleur d'interruption que l'on commence à traiter l'interruption
    outb(0x20, 0x20);


    //TODO: Rendre éventuellement ce procédé plus propre en utilisant qu'une variable (inter) et moins de if
    inter++; // On compte une interruption "globale"

    // On calcule le temps heure, minute, seconde en fonction du nombre d'interruptions
    frac_sec++;
    if(frac_sec >= CLOCKFREQ){
        frac_sec = 0;
        sec++;
        if(sec >= 60){
            sec = 0;
            min++;
            if(min >= 60){
                min = 0;
                hour++;
                if(hour >= 24){
                    hour = 0;
                }
            }
        }
    }
    
    
    scheduler_delay++;

    if(scheduler_delay >= CLOCKFREQ / SCHEDULER_FREQUENCY){
        scheduler_delay = 0;
        schedule();
    }
    
    clock_tick();

    char s[9]; //On prépare la sortie sous la forme HH:MM:SS
    s[8] = '\0';

    sprintf(s, "%02d:%02d:%02d", hour, min, sec);
    print_top_right(s);
}

/*
    Cette fonction initialise les réglages de l'horloge en fonction des constantes système
*/
void clock_init(){
    outb(CLOCK_INIT_COMMAND, CLOCK_CONTROL_PORT);
    outb(CLOCKPERIOD & 0x00FF, CLOCK_DATA_PORT);
    outb(((CLOCKPERIOD & 0xFF00) >> 8) & 0xFF, CLOCK_DATA_PORT);
}

/*
    Cette fonction est une primitive système qui renseigne sur les réglages de l'horloge (les pointeurs sont supposés être dans un espace d'écriture approprié)
*/
void clock_settings(unsigned long *quartz, unsigned long *ticks){
    *quartz = QUARTZ;
    *ticks = CLOCKPERIOD;
}

/*
    Cette fonction est une primitive système qui retourne le nombre d'interruptions d'horloge ayant eu lieu depuis le démarrage du système
*/
unsigned long current_clock(){
    return inter;
}