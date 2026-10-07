/* ============================================================
 *  Tests du module Fritax Moniteur - sur le VRAI /proc de la machine.
 *  On appelle le module de lecture et on verifie que les valeurs
 *  sont plausibles (memoire > 0, CPU entre 0 et 100, uptime > 0...).
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "proc.h"
#include "screen.h"
#include "app.h"

static int reussis, rates;

static void verifie(int condition, const char *quoi) {
    if (condition) { reussis++; printf("  ok   %s\n", quoi); }
    else           { rates++;   printf("  RATE %s\n", quoi); }
}

int main(void) {
    printf("Tests du module Fritax Moniteur (lecture reelle de /proc)\n");

    /* --- lecture brute de /proc/stat --- */
    long long total = 0, inactif = 0;
    verifie(fx_mon_cpu_brut(&total, &inactif) == 0, "/proc/stat lisible");
    verifie(total > 0, "le total de jiffies CPU est > 0");
    verifie(inactif >= 0 && inactif <= total, "l'inactif tient entre 0 et le total");

    /* --- fx_mon_lire remplit la structure --- */
    fx_mon_reinit();
    FXMonInfo a;
    verifie(fx_mon_lire(&a) == 0, "fx_mon_lire lit /proc sans erreur");
    verifie(a.mem_total_mo > 0, "memoire totale > 0 Mo");
    verifie(a.mem_dispo_mo > 0, "memoire disponible > 0 Mo");
    verifie(a.mem_dispo_mo <= a.mem_total_mo, "memoire disponible <= totale");
    verifie(a.mem_utilisee_mo >= 0, "memoire utilisee >= 0");
    verifie(a.swap_libre_mo <= a.swap_total_mo, "swap libre <= swap total");
    verifie(a.uptime_s > 0, "uptime > 0 seconde");
    verifie(a.nb_coeurs >= 1, "au moins 1 processeur logique detecte");
    verifie(a.charge1 >= 0 && a.charge5 >= 0 && a.charge15 >= 0, "les trois charges >= 0");
    verifie(strlen(a.cpu_modele) > 0, "le modele du processeur n'est pas vide");
    printf("  infos : %s | %d coeurs | %.0f Mo | uptime %.0f s\n",
           a.cpu_modele, a.nb_coeurs, a.mem_total_mo, a.uptime_s);

    /* --- le CPU est calcule ENTRE DEUX RELEVES --- */
    verifie(a.cpu_pct == 0.0, "le premier releve CPU vaut 0 %% (aucun precedent)");
    volatile long boucle = 0;
    for (long i = 0; i < 40000000L; i++) boucle += i;   /* on sollicite le CPU */
    usleep(200000);
    FXMonInfo b;
    verifie(fx_mon_lire(&b) == 0, "deuxieme releve de /proc ok");
    verifie(b.cpu_pct >= 0.0 && b.cpu_pct <= 100.0, "le taux CPU tient entre 0 et 100 %");
    printf("      (taux mesure : %.1f %%)\n", b.cpu_pct);
    (void)boucle;

    /* --- l'application sait se dessiner (verifie app.c + la boite a outils) --- */
    FXScreen *s = fx_screen_new(800, 600);
    if (s) {
        fx_mon_init(800, 600, NULL);
        fx_mon_demo();                 /* valeurs factices : pas de /proc ici */
        fx_mon_draw(s);
        verifie(1, "le dessin de la fenetre ne plante pas (800x600)");
        fx_screen_free(s);
    } else {
        verifie(0, "framebuffer 800x600 cree");
    }

    printf("\n%d reussis, %d rates\n", reussis, rates);
    return rates ? 1 : 0;
}
