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

#include "m_file.h"


// initialiser mutex et cond
int initialiser_mutex(pthread_mutex_t *pmutex){
    pthread_mutexattr_t mutexattr;
    int code;
    if( ( code = pthread_mutexattr_init(&mutexattr) ) != 0)	    
        return code;

    if( ( code = pthread_mutexattr_setpshared(&mutexattr,
                PTHREAD_PROCESS_SHARED) ) != 0)	    
        return code;
    code = pthread_mutex_init(pmutex, &mutexattr)  ;
    return code;
}
int initialiser_cond(pthread_cond_t *pcond){
    pthread_condattr_t condattr;
    int code;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr,PTHREAD_PROCESS_SHARED);
    if( ( code = pthread_cond_init(pcond, &condattr) ) != 0)
      return code;
    code = pthread_cond_init(pcond, &condattr)  ;
    return code;
}

// connexion a une file de message ou creation
MESSAGE *m_connexion(const char *nom, int options, 
    size_t nb_msg, size_t len_max, mode_t mode){

    return NULL;
}

// deconnection de la file 
int m_deconnexion(MESSAGE *file){

    return -1;
}

// destruction de la file
int m_destruction(const char *nom){

    return -1;
}

// envoie de message
int m_envoi(MESSAGE *file, const void *msg, 
    size_t len, int msgflag){

    return -1;
}

// lire message
ssize_t m_reception(MESSAGE *file, void *msg, 
    size_t len, long type, int flags){

    return 0;
}

// t taille 
size_t m_message_len(MESSAGE *file){

    return 0;
}
size_t m_capacite(MESSAGE *file){

    return 0;
}
size_t m_nb(MESSAGE *file){

    return 0;
}

int main(int argc, char const *argv[]){

    

    return 0;
}