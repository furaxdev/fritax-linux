/* ============================================================
 *  Fritax Moniteur - lecture du VRAI /proc
 *
 *  Module separe et testable : aucune dependance externe,
 *  juste la libc. Toutes les valeurs viennent des fichiers
 *  /proc de la machine (stat, meminfo, loadavg, uptime, cpuinfo).
 * ============================================================ */
#ifndef FRITAX_MON_PROC_H
#define FRITAX_MON_PROC_H

/* Un instantane complet du systeme. */
typedef struct {
    char   cpu_modele[128];   /* modele du processeur (/proc/cpuinfo) */
    int    nb_coeurs;         /* nombre de processeurs logiques */
    double cpu_pct;           /* occupation CPU 0..100, entre deux relevés */

    double mem_total_mo;      /* memoire vive totale, en Mo */
    double mem_dispo_mo;      /* memoire disponible, en Mo */
    double mem_utilisee_mo;   /* memoire utilisee = total - disponible */
    double swap_total_mo;     /* memoire d'echange totale, en Mo */
    double swap_libre_mo;     /* memoire d'echange libre, en Mo */

    double charge1, charge5, charge15;   /* charge moyenne (/proc/loadavg) */
    double uptime_s;                     /* secondes depuis le demarrage */
} FXMonInfo;

/* Remplit `info` avec les valeurs REELLES lues dans /proc.
   Le pourcentage CPU est calcule ENTRE DEUX RELEVES : le premier appel
   renvoie 0 % (pas encore de precedent), les suivants comparent avec le
   releve precedent. Renvoie 0 si les fichiers essentiels ont pu etre lus,
   -1 sinon. */
int fx_mon_lire(FXMonInfo *info);

/* Oublie le releve CPU precedent : le prochain appel repart a 0 %. */
void fx_mon_reinit(void);

/* Lit l'etat brut du processeur (/proc/stat, ligne "cpu ").
   total = somme des jiffies, inactif = idle + iowait.
   Expose separement pour les tests. Renvoie 0 si ok, -1 sinon. */
int fx_mon_cpu_brut(long long *total, long long *inactif);

#endif
