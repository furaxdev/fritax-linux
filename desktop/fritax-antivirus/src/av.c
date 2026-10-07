/* ============================================================
 *  FRITAX ANTIVIRUS - le scanner de la distribution
 *
 *  Ecrit a la main, sans aucune bibliotheque externe (juste la libc).
 *  Il ne remplace evidemment pas un vrai antivirus : c'est un scanner
 *  de signatures, comme ClamAV dans son principe. Sa force c'est qu'il
 *  est lisible de bout en bout, et qu'il est teste avec le fichier
 *  standard de l'industrie (EICAR), celui que TOUS les antivirus
 *  doivent detecter.
 *
 *  Usage :
 *    fritax-av --liste                     affiche la base de signatures
 *    fritax-av [dossier]                   analyse (defaut : /home)
 *    fritax-av --quarantaine [dossier]     analyse ET met en quarantaine
 *    fritax-av --test                      auto-test (cree et detecte EICAR)
 * ============================================================ */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define VERSION "1.0"
#define QUARANTAINE_DEF "/var/lib/fritax-antivirus/quarantaine"
#define TAILLE_MAX_FICHIER (8u * 1024u * 1024u)   /* au dela, on ne lit pas (8 Mo) */
#define MAX_TROUVES 512

/* ---------------------------------------------------------------
 *  BASE DE SIGNATURES
 *  Chaque signature = un nom + un motif + une gravite + une explication.
 *  Les motifs sont ceux des malwares Linux les plus courants (mineurs de
 *  cryptomonnaie, chevaux de Troie d'acces, telechargeurs) et le fichier
 *  de test standard EICAR.
 * --------------------------------------------------------------- */
typedef struct {
    const char *nom;
    const char *motif;
    int         gravite;      /* 3 = dangereux, 2 = suspect, 1 = a verifier */
    const char *explication;
} Signature;

static const Signature BASE[] = {
    { "EICAR-Test-File", "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR", 3,
      "Fichier de test standard des antivirus. Aucun danger, sert a prouver que le scanner detecte." },
    { "Miner.XMRig", "stratum+tcp://", 3,
      "Adresse de pool de minage : un programme mine de la cryptomonnaie a votre insu." },
    { "Miner.XMRig.2", "xmrig", 3,
      "Nom du mineur de cryptomonnaie le plus repandu sur les serveurs Linux." },
    { "Miner.Monero", "--donate-level=", 2,
      "Option typique d'un mineur installe sans autorisation." },
    { "Backdoor.Bash-TCP", "/dev/tcp/", 3,
      "Ouverture d'une connexion reseau directement depuis bash : porte derobee classique." },
    { "Backdoor.Netcat", "nc -e /bin/sh", 3,
      "Netcat en mode execution de shell : donne un acces complet a distance." },
    { "Backdoor.Netcat.2", "ncat -e /bin/bash", 3,
      "Variante de la meme porte derobee." },
    { "Dropper.PipeShell", "| sh", 2,
      "Un telechargement envoye directement dans un shell : installation a l'aveugle." },
    { "Dropper.PipeBash", "| bash", 2,
      "Meme chose avec bash : tres courant dans les installations par script." },
    { "Dropper.Base64", "base64 -d | ", 3,
      "Commande dissimulee derriere un encodage : technique classique pour passer inapercu." },
    { "Dropper.Wget.Sh", "wget -qO- ", 2,
      "Telechargement silencieux puis execution probable." },
    { "Persistance.Cron", "*/1 * * * * curl", 3,
      "Tache planifiee toutes les minutes qui retelcharge quelque chose : persistance de malware." },
    { "Persistance.Profile", "LD_PRELOAD=/tmp/", 3,
      "Bibliotheque chargee depuis /tmp : technique de detournement classique." },
    { "Reverse.Perl", "perl -e 'use Socket", 3,
      "Reverse shell ecrit en Perl." },
    { "Reverse.Python", "socket.socket(socket.AF_INET", 2,
      "Connexion reseau sortante brute : a verifier s'il n'y a pas de raison legitime." },
    { "Keylogger.Xinput", "keylog", 1,
      "Nom evoquant un enregistreur de frappes. A verifier vous-meme." },
};

static const int NB_SIG = (int)(sizeof(BASE) / sizeof(BASE[0]));

/* ---------------------------------------------------------------
 *  Couleurs ANSI (le terminal de Fritax les comprend)
 * --------------------------------------------------------------- */
#define ROUGE   "\033[31m"
#define VERT    "\033[32m"
#define JAUNE   "\033[33m"
#define BLEU    "\033[34m"
#define VIOLET  "\033[35m"
#define CYAN    "\033[36m"
#define GRAS    "\033[1m"
#define FIN     "\033[0m"

typedef struct {
    char chemin[1024];
    const char *nom;
    const char *explication;
    int  gravite;
} Trouve;

static Trouve trouves[MAX_TROUVES];
static int    nb_trouves = 0;
static unsigned long fichiers_vus = 0, octets_vus = 0;

/* ---------------------------------------------------------------
 *  RECHERCHE DE MOTIF
 *  Recherche naive, volontairement simple et lisible. On saute d'un
 *  cran des qu'on trouve un debut de correspondance, c'est suffisant
 *  pour des motifs de quelques dizaines d'octets.
 * --------------------------------------------------------------- */
static int contient(const char *donnees, size_t taille, const char *motif)
{
    size_t n = strlen(motif);
    if (n == 0 || n > taille) return 0;
    for (size_t i = 0; i + n <= taille; i++) {
        if (donnees[i] == motif[0] && memcmp(donnees + i, motif, n) == 0)
            return 1;
    }
    return 0;
}

/* Est-ce un fichier qu'on peut analyser ? (pas un dossier ni un lien) */
static int analysable(const char *chemin)
{
    struct stat st;
    if (lstat(chemin, &st) != 0) return 0;
    if (!S_ISREG(st.st_mode)) return 0;          /* pas un fichier normal */
    if (st.st_size <= 0 || (unsigned long)st.st_size > TAILLE_MAX_FICHIER) return 0;
    return 1;
}

static void ajouter_trouve(const char *chemin, const Signature *sig)
{
    if (nb_trouves >= MAX_TROUVES) return;
    snprintf(trouves[nb_trouves].chemin, sizeof trouves[0].chemin, "%s", chemin);
    trouves[nb_trouves].nom = sig->nom;
    trouves[nb_trouves].explication = sig->explication;
    trouves[nb_trouves].gravite = sig->gravite;
    nb_trouves++;
}

/* ---------------------------------------------------------------
 *  MISE EN QUARANTAINE
 *  On DEPLACE le fichier (on ne le supprime jamais : on ne detruit
 *  pas les donnees de quelqu'un sur une simple detection).
 * --------------------------------------------------------------- */
static int mettre_en_quarantaine(const char *chemin, const char *dossier_quarantaine)
{
    mkdir(dossier_quarantaine, 0700);
    char destination[1200];
    snprintf(destination, sizeof destination, "%s/%ld_%s",
             dossier_quarantaine, (long)time(NULL), strrchr(chemin, '/') ? strrchr(chemin, '/') + 1 : "fichier");
    if (rename(chemin, destination) == 0) {
        /* petite fiche pour savoir d'ou venait le fichier */
        char fiche[1300];
        snprintf(fiche, sizeof fiche, "%s.txt", destination);
        FILE *f = fopen(fiche, "w");
        if (f) { fprintf(f, "origine: %s\n", chemin); fclose(f); }
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------
 *  ANALYSE D'UN FICHIER
 * --------------------------------------------------------------- */
static void analyser_fichier(const char *chemin, int avec_quarantaine, const char *dossier_quarantaine)
{
    if (!analysable(chemin)) return;

    FILE *f = fopen(chemin, "rb");
    if (!f) return;
    static char tampon[TAILLE_MAX_FICHIER];
    size_t lu = fread(tampon, 1, sizeof tampon, f);
    fclose(f);
    if (lu == 0) return;

    fichiers_vus++;
    octets_vus += lu;

    for (int i = 0; i < NB_SIG; i++) {
        if (contient(tampon, lu, BASE[i].motif)) {
            ajouter_trouve(chemin, &BASE[i]);
            printf("  " ROUGE "TROUVE" FIN "  %s\n", chemin);
            printf("         %s%s%s (gravite %d) - %s\n",
                   GRAS, BASE[i].nom, FIN, BASE[i].gravite, BASE[i].explication);
            if (avec_quarantaine) {
                if (mettre_en_quarantaine(chemin, dossier_quarantaine))
                    printf("         " JAUNE "-> deplace en quarantaine (le fichier est conserve)" FIN "\n");
                else
                    printf("         " JAUNE "-> quarantaine impossible (%s)" FIN "\n", strerror(errno));
            }
            return;   /* une seule detection par fichier suffit */
        }
    }
}

/* ---------------------------------------------------------------
 *  PARCOURS RECURSIF
 * --------------------------------------------------------------- */
static void parcourir(const char *dossier, int avec_quarantaine, const char *dossier_quarantaine, int profondeur)
{
    if (profondeur > 24) return;                 /* garde-fou contre les liens bizarres */

    DIR *d = opendir(dossier);
    if (!d) return;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        /* on ne descend pas dans ces faux systemes de fichiers */
        if (strcmp(e->d_name, "proc") == 0 || strcmp(e->d_name, "sys") == 0 ||
            strcmp(e->d_name, "dev") == 0  || strcmp(e->d_name, "run") == 0) continue;

        char chemin[1400];
        snprintf(chemin, sizeof chemin, "%s/%s", dossier, e->d_name);

        struct stat st;
        if (lstat(chemin, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            parcourir(chemin, avec_quarantaine, dossier_quarantaine, profondeur + 1);
        } else if (S_ISREG(st.st_mode)) {
            analyser_fichier(chemin, avec_quarantaine, dossier_quarantaine);
        }
    }
    closedir(d);
}

/* ---------------------------------------------------------------
 *  AUTO-TEST avec le fichier standard EICAR
 *  C'est ce qui prouve que le scanner marche vraiment : n'importe quel
 *  antivirus du monde doit detecter ce fichier-la.
 * --------------------------------------------------------------- */
static int auto_test(void)
{
    const char *motif = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*";
    char dossier[] = "/tmp/fritax-av-test-XXXXXX";
    if (!mkdtemp(dossier)) { printf("  impossible de creer le dossier de test\n"); return 1; }

    char propre[256], infecte[256];
    snprintf(propre, sizeof propre, "%s/fichier_propre.txt", dossier);
    snprintf(infecte, sizeof infecte, "%s/fichier_infecte.txt", dossier);

    FILE *f = fopen(propre, "w");
    fprintf(f, "Ceci est un fichier tout a fait normal.\n"); fclose(f);
    f = fopen(infecte, "w");
    fprintf(f, "%s\n", motif); fclose(f);

    printf(GRAS "  Auto-test de Fritax Antivirus\n" FIN);
    printf("  fichier propre    : %s\n", propre);
    printf("  fichier infecte   : %s (fichier de test EICAR)\n\n", infecte);

    nb_trouves = 0;
    analyser_fichier(propre, 0, NULL);
    int detecte_propre = nb_trouves;
    nb_trouves = 0;
    analyser_fichier(infecte, 0, NULL);
    int detecte_infecte = nb_trouves;

    printf("\n");
    if (detecte_propre == 0 && detecte_infecte == 1) {
        printf("  " VERT GRAS "AUTO-TEST REUSSI" FIN " : le fichier propre est ignore, le fichier EICAR est detecte.\n");
    } else {
        printf("  " ROUGE GRAS "AUTO-TEST EN ECHEC" FIN " : propre=%d (attendu 0), infecte=%d (attendu 1)\n",
               detecte_propre, detecte_infecte);
    }
    unlink(propre); unlink(infecte); rmdir(dossier);
    return (detecte_propre == 0 && detecte_infecte == 1) ? 0 : 1;
}

static void afficher_base(void)
{
    printf(GRAS "  Base de signatures de Fritax Antivirus 1.0" FIN "  (%d signatures)\n\n", NB_SIG);
    for (int i = 0; i < NB_SIG; i++) {
        const char *c = BASE[i].gravite == 3 ? ROUGE : (BASE[i].gravite == 2 ? JAUNE : CYAN);
        printf("  %s%-20s" FIN " gravite %d   motif : \"%s\"\n",
               c, BASE[i].nom, BASE[i].gravite, BASE[i].motif);
        printf("      %s\n", BASE[i].explication);
    }
    printf("\n  Gravite 3 = dangereux, 2 = suspect, 1 = a verifier soi-meme.\n");
    printf("  Ce scanner ne remplace pas un vrai antivirus : il cherche des\n");
    printf("  motifs connus dans les fichiers, comme ClamAV dans son principe.\n");
}

static void aide(const char *prog)
{
    printf(GRAS "  Fritax Antivirus " VERSION FIN " - le scanner de la distribution Fritax\n\n");
    printf("  Usage :\n");
    printf("    %s                      analyse /home\n", prog);
    printf("    %s [dossier]            analyse le dossier donne\n", prog);
    printf("    %s --quarantaine [dir]  analyse et met en quarantaine\n", prog);
    printf("    %s --liste              affiche la base de signatures\n", prog);
    printf("    %s --test               auto-test avec le fichier standard EICAR\n", prog);
    printf("    %s --aide               cette page\n\n", prog);
    printf("  Les fichiers detects ne sont JAMAIS supprimes : en quarantaine ils\n");
    printf("  sont seulement deplaces, avec une fiche indiquant leur origine.\n");
}

int main(int argc, char **argv)
{
    const char *cible = "/home";
    int quarantaine = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0)         return auto_test();
        if (strcmp(argv[i], "--liste") == 0)        { afficher_base(); return 0; }
        if (strcmp(argv[i], "--aide") == 0 || strcmp(argv[i], "-h") == 0) { aide(argv[0]); return 0; }
        if (strcmp(argv[i], "--quarantaine") == 0)  { quarantaine = 1; continue; }
        if (argv[i][0] == '-') { printf("  option inconnue : %s (essayez --aide)\n", argv[i]); return 2; }
        cible = argv[i];
    }

    struct stat st;
    if (stat(cible, &st) != 0) {
        printf("  dossier introuvable : %s\n", cible);
        return 2;
    }

    printf("\n" GRAS VIOLET "  FRITAX ANTIVIRUS " VERSION FIN "\n");
    printf("  analyse de : " CYAN "%s" FIN "\n", cible);
    printf("  %d signatures chargees %s\n\n", NB_SIG,
           quarantaine ? "(mise en quarantaine ACTIVE)" : "(detection seule)");
    fflush(stdout);

    parcourir(cible, quarantaine, QUARANTAINE_DEF, 0);

    printf("\n" GRAS "  Rapport" FIN "\n");
    printf("    fichiers analyses : %lu\n", fichiers_vus);
    printf("    donnees lues      : %.1f Mo\n", (double)octets_vus / (1024.0 * 1024.0));
    if (nb_trouves == 0) {
        printf("    " VERT "aucune menace detectee" FIN "\n\n");
        return 0;
    }
    int graves = 0;
    for (int i = 0; i < nb_trouves; i++) if (trouves[i].gravite == 3) graves++;
    printf("    " ROUGE "%d fichier(s) suspect(s)" FIN ", dont %d de gravite maximale\n", nb_trouves, graves);
    if (!quarantaine)
        printf("    Relancez avec --quarantaine pour les mettre de cote (aucune suppression).\n");
    printf("\n");
    return nb_trouves > 0 ? 1 : 0;
}
