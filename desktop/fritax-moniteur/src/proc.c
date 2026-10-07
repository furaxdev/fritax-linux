/* ============================================================
 *  FRITAX MONITEUR - lecture des donnees systeme dans /proc
 *
 *  Ecrit a la main, sans aucune bibliotheque externe (juste la libc).
 *  On lit reellement : /proc/stat, /proc/meminfo, /proc/loadavg,
 *  /proc/uptime et /proc/cpuinfo. Rien n'est simule ici.
 * ============================================================ */
#define _GNU_SOURCE
#include "proc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Releve CPU precedent (jiffies bruts) : c'est lui qui permet de calculer
   le taux d'occupation ENTRE DEUX RELEVES plutot qu'une moyenne depuis
   le demarrage (qui resterait quasi constante). */
static long long av_total = -1, av_inactif = 0;

/* kilo-octets -> "Mo" lisibles (1 Mo = 1024 ko). */
#define MO(ko) ((double)(ko) / 1024.0)

/* enleve les espaces de debut et de fin d'une chaine, sur place. */
static void trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
}

/* ---------------------------------------------------------------
 *  /proc/stat  ->  jiffies CPU bruts
 *  Premiere ligne : "cpu  user nice system idle iowait irq softirq ..."
 *  total   = somme de tous les champs
 *  inactif = idle + iowait
 * --------------------------------------------------------------- */
int fx_mon_cpu_brut(long long *total, long long *inactif) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;
    char ligne[512];
    int ok = 0;
    if (fgets(ligne, sizeof ligne, f) && strncmp(ligne, "cpu ", 4) == 0) {
        long long champs[16];
        int n = 0;
        char *p = ligne + 4;
        while (n < 16) {
            char *fin;
            long long v = strtoll(p, &fin, 10);
            if (fin == p) break;          /* plus aucun nombre */
            champs[n++] = v;
            p = fin;
        }
        if (n >= 4) {
            long long t = 0;
            for (int i = 0; i < n; i++) t += champs[i];
            *total   = t;
            *inactif = champs[3] + (n > 4 ? champs[4] : 0);   /* idle + iowait */
            ok = 1;
        }
    }
    fclose(f);
    return ok ? 0 : -1;
}

/* /proc/meminfo -> memoire et swap, convertis en Mo. */
static int lire_meminfo(FXMonInfo *o) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    char ligne[256];
    while (fgets(ligne, sizeof ligne, f)) {
        long long v;
        if      (sscanf(ligne, "MemTotal: %lld", &v) == 1)     o->mem_total_mo  = MO(v);
        else if (sscanf(ligne, "MemAvailable: %lld", &v) == 1) o->mem_dispo_mo  = MO(v);
        else if (sscanf(ligne, "SwapTotal: %lld", &v) == 1)    o->swap_total_mo = MO(v);
        else if (sscanf(ligne, "SwapFree: %lld", &v) == 1)     o->swap_libre_mo = MO(v);
    }
    fclose(f);
    return 0;
}

/* /proc/loadavg -> charge sur 1, 5 et 15 minutes. */
static void lire_loadavg(FXMonInfo *o) {
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) return;
    if (fscanf(f, "%lf %lf %lf", &o->charge1, &o->charge5, &o->charge15) != 3) {
        o->charge1 = o->charge5 = o->charge15 = 0;
    }
    fclose(f);
}

/* /proc/uptime -> temps de fonctionnement, en secondes. */
static void lire_uptime(FXMonInfo *o) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) return;
    if (fscanf(f, "%lf", &o->uptime_s) != 1) o->uptime_s = 0;
    fclose(f);
}

/* ---------------------------------------------------------------
 *  /proc/cpuinfo -> modele du processeur + nombre de coeurs.
 *  Selon l'architecture la ligne du modele s'appelle "model name"
 *  (x86), "Hardware" / "Processor" (ARM)... on prend la plus precise.
 * --------------------------------------------------------------- */
static void lire_cpuinfo(FXMonInfo *o) {
    o->nb_coeurs = 0;
    snprintf(o->cpu_modele, sizeof o->cpu_modele, "Processeur inconnu");
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;
    char ligne[512];
    int prio = 0;   /* 0 rien, 1 "Processor", 2 "Hardware"/"cpu model", 3 "model name" */
    while (fgets(ligne, sizeof ligne, f)) {
        char *c = strchr(ligne, ':');
        if (!c) continue;
        *c = 0;
        char *val = c + 1;
        trim(ligne);
        trim(val);
        if (!strcmp(ligne, "processor") && *val) { o->nb_coeurs++; continue; }
        int p = 0;
        if      (!strcmp(ligne, "model name")) p = 3;
        else if (!strcmp(ligne, "Hardware"))   p = 2;
        else if (!strcmp(ligne, "cpu model"))  p = 2;
        else if (!strcmp(ligne, "Processor"))  p = 1;
        if (p > prio && *val) {
            prio = p;
            snprintf(o->cpu_modele, sizeof o->cpu_modele, "%s", val);
        }
    }
    fclose(f);
    if (o->nb_coeurs < 1) o->nb_coeurs = 1;
}

void fx_mon_reinit(void) {
    av_total = -1;
    av_inactif = 0;
}

int fx_mon_lire(FXMonInfo *o) {
    if (!o) return -1;
    memset(o, 0, sizeof *o);

    /* processeur : taux calcule entre le releve precedent et celui-ci */
    long long total, inactif;
    if (fx_mon_cpu_brut(&total, &inactif) != 0) return -1;
    if (av_total >= 0 && total > av_total) {
        long long dt = total - av_total;
        long long di = inactif - av_inactif;
        if (di < 0) di = 0;
        if (di > dt) di = dt;
        o->cpu_pct = 100.0 * (double)(dt - di) / (double)dt;
    }
    av_total = total;
    av_inactif = inactif;

    /* memoire (essentielle) */
    if (lire_meminfo(o) != 0) return -1;
    if (o->mem_total_mo <= 0) return -1;
    o->mem_utilisee_mo = o->mem_total_mo - o->mem_dispo_mo;
    if (o->mem_utilisee_mo < 0) o->mem_utilisee_mo = 0;

    /* le reste, au mieux */
    lire_loadavg(o);
    lire_uptime(o);
    lire_cpuinfo(o);
    return 0;
}
