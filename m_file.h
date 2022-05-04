#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>

#ifndef MFILE_H
#define MFILE_H

#define NBSIG 10
#define NBTYPE 32

typedef struct{
    long type;
    long len;
    char mtext[];
}mon_message;

typedef struct{
    long pid;
    long typeMess;
    int typeSignal;
}signalEnregis;

typedef struct{
    long typeMess;
    long cb;
}je_suis_bloque;

typedef struct{
    size_t longMax;
    size_t capacite;
    int first;
    int last;
    int lastSignal;
    int lastBloque;
    pthread_mutex_t mutex;
    char enregistrement[NBSIG*sizeof(signalEnregis)];
    char bloque[NBTYPE*sizeof(je_suis_bloque)];
    char messages[];
}enteteFile;

typedef struct{
    enteteFile *file;
    // signalTab *signal;
    long type;
}MESSAGE;



void handler(int sig);
extern int initialiser_mutex(pthread_mutex_t *pmutex);
extern int initialiser_cond(pthread_cond_t *pcond);

extern MESSAGE *m_connexion( const char *nom, int options, int nb, ...);//size_t nb_msg, size_t len_max, mode_t mode
extern int m_deconnexion(MESSAGE *file);
extern int m_destruction(const char *nom);
extern int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag);
extern void suppressionMess(MESSAGE *file, size_t taille, size_t deb);
extern ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags,int tour);
extern size_t m_message_len(MESSAGE *file);
extern size_t m_capacite(MESSAGE *file);
extern size_t m_nb(MESSAGE *file);
extern size_t m_nbSignal(MESSAGE *file);
extern size_t m_size_messages(MESSAGE *file);
extern int enregistrement(MESSAGE *file, int signal, long type);
extern void suppressionSig(MESSAGE *file, int pos);
extern int desenregistrement(MESSAGE *file);
extern void affichage_message(MESSAGE *m);
extern void affichage_mon_message(mon_message *m);
extern void affichage_entete(enteteFile *e, size_t nb, size_t l, size_t nbS, size_t nbT);
// extern void affichage_signal(signalTab *s, size_t nb);
// extern void affichage_mon_mess(mon_message *mm);


#endif
