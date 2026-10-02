/* ============================================================
 *  FRITAX IPC - demander au bureau d'ouvrir une fenetre
 *  Le bureau ecoute sur une prise Unix locale ; n'importe quel
 *  programme (ou le lanceur) peut lui demander d'ouvrir une appli.
 *  Aucun serveur graphique, aucune bibliotheque : juste un socket.
 * ============================================================ */
#ifndef FRITAX_IPC_H
#define FRITAX_IPC_H

#define FX_IPC_PATH "/tmp/fritax.sock"

/* cote bureau */
void fx_ipc_init(const char *path);   /* cree la prise d'ecoute (path = NULL -> FX_IPC_PATH) */
void fx_ipc_poll(void);               /* traite les demandes arrivees (non bloquant) */
const char *fx_ipc_path(void);

/* cote client : renvoie 0 si le bureau a accepte, -1 sinon */
int  fx_ipc_ask(const char *requete);

#endif
