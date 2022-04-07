#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>

#ifndef MFILE_H
#define MFILE_H

typedef struct{
    long type;
    char mtext[];
}mon_message;

typedef struct{
    size_t longMax;
    size_t capacite;
    int first;
    int last;
    pthread_mutex_t mutex;
    mon_message *tabMessage;
}enteteFile;

typedef struct{
    long type;
    enteteFile *file;
}MESSAGE;




extern int initialiser_mutex(pthread_mutex_t *pmutex);
extern int initialiser_cond(pthread_cond_t *pcond);

extern MESSAGE *m_connexion( const char *nom, int options, int nb, ...);//size_t nb_msg, size_t len_max, mode_t mode
extern int m_deconnexion(MESSAGE *file);
extern int m_destruction(const char *nom);
extern int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag);
extern ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags);
extern size_t m_message_len(MESSAGE *file);
extern size_t m_capacite(MESSAGE *file);
extern size_t m_nb(MESSAGE *file);
extern void affichage_message(MESSAGE *m);
extern void affichage_entete(enteteFile *e);
extern void affichage_mon_mess(mon_message mm);


#endif
