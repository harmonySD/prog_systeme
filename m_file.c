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

#include "m_file.h"

void handler(int sig){
    char *txt="\n*NOTIFICATION* Message arrive !\n\n";
    write(1,txt,strlen(txt));  
}

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
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    if( ( code = pthread_cond_init(pcond, &condattr) ) != 0)
      return code;
    code = pthread_cond_init(pcond, &condattr)  ;
    return code;
}

// taille dun message maximal 
size_t m_message_len(MESSAGE *file){
    return file->file->longMax;
}
// taille reel dun mon_message 
size_t m_message_taille(mon_message *m){
    return (sizeof(mon_message) + m->len);
}
// taille maximal dun mon_message
size_t m_size_messages(MESSAGE *file){
    return (sizeof(mon_message) + file->file->longMax);
}
// capacite message enregistrer
size_t m_capacite(MESSAGE *file){
    return file->file->capacite;
}
// nombre de message enregistrer
size_t m_nb(MESSAGE *file){
    int nb = 0;
    size_t emplacement = 0;
    size_t tailleMax = m_size_messages(file);
    for(int i=0; i < file->file->capacite; i++){
        // ne pas depasser last qui signifie le dernier message enregistrer
        if(emplacement < file->file->last){
            char buf[tailleMax];
            for(int j=0; j < tailleMax; j++){
                buf[j] = file->file->messages[j + emplacement];
            }
            mon_message * mess = (mon_message*)buf;
            nb++;
            emplacement += sizeof(mon_message) + mess->len; 
        }        
    }
    return nb;
}
// nombre de processus enregistrer 
size_t m_nbSignal(MESSAGE *file){
    return (file->file->lastSignal)/sizeof(signalEnregis);
}
//nombre de signaux enregistree
size_t m_nbType(MESSAGE *file){
    return(file->file->lastBloque)/sizeof(je_suis_bloque);
}
// taille dun signalEnregis
size_t m_size_signal(MESSAGE *file){
    return(sizeof(signalEnregis));
}

// connexion a une file de message ou creation
MESSAGE *m_connexion(const char *nom, int options, int nb, ...){ 
    //size_t nb_msg, size_t len_max, mode_t mode
    // nb = 0 ou 3 -> nb darguments en plus
    MESSAGE *mess = malloc(sizeof(MESSAGE));
    enteteFile *entete;
    //option ne contient pas ocreat
    if((options == O_RDONLY)||(options == O_WRONLY)||(options == O_RDWR)){
        // on ne cree pas la file on a que 2 arg 
        int d = shm_open(nom, options, S_IRUSR | S_IWUSR);
        if(d < 0) return NULL;
        struct stat buf;
        int r = fstat(d,&buf);
        if(r == -1) return NULL;
        entete = mmap(NULL, buf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, d, 0);
        mess->type = buf.st_mode;
        mess->file = entete;        
    }else{
        va_list param;
        va_start(param, nb);
        size_t nb_msg;
        size_t len_max;
        mode_t mode;
        nb_msg = (size_t) va_arg(param, size_t);
        len_max = (size_t) va_arg(param, size_t);
        mode = (mode_t)va_arg(param, int);
        size_t tailleent = sizeof(enteteFile) + (sizeof(mon_message) + len_max)*nb_msg
                    + sizeof(signalEnregis)*NBSIG + sizeof(je_suis_bloque)*NBTYPE;
        if(nom == NULL){
            // file ano
            entete = mmap(NULL, tailleent, PROT_READ|PROT_WRITE, 
                        MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        }else {
            // file avec un nom
            int d = shm_open(nom, options, mode);
            ftruncate(d, sizeof(enteteFile));
            if(d < 0) return NULL;
            entete = mmap(NULL, tailleent, PROT_READ|PROT_WRITE, MAP_SHARED, d, 0);
        }
        // initialisation du MESSAGE et de lentete
        int r = initialiser_mutex(&entete->mutex);
        if(r == -1) return NULL;
        r = initialiser_cond(&entete->env);
        if(r == -1) return NULL;
        r = initialiser_cond(&entete->rec);
        if(r == -1) return NULL;
        entete->capacite = nb_msg;
        entete->first = 0;
        entete->last = 0;
        entete->lastSignal = 0;
        entete->lastBloque =0;
        entete->longMax = len_max;
        mess->type = mode;
        mess->file = entete;
        msync(&mess->file, sizeof(mess->file), MS_SYNC);
        if(r == -1) return NULL;
        va_end(param);
    }
    return mess;   
}

// deconnection de la file 
int m_deconnexion(MESSAGE *file){
    if(munmap(file->file, 
        sizeof(enteteFile) + m_size_messages(file) * file->file->capacite 
            + sizeof(signalEnregis)*NBSIG+sizeof(je_suis_bloque)*NBTYPE) == -1)
        return -1;    
    return 0;
}

//destruction de la file
int m_destruction(const char *nom){
    if(shm_unlink(nom) == -1) return -1;
    return 0;
}

// envoie de message
int m_envoi(MESSAGE *file, const void *msg, size_t len, int msgflag){
    if(!(file->type & S_IWUSR)) return -1;
    // tableau de message non plein
    if(m_nb(file) < file->file->capacite){
        // verifier len pas trop grand
        if(file->file->longMax < len) return -1;
        int r = pthread_mutex_lock(&file->file->mutex);
        if(r == -1) return -1;
        memcpy(file->file->messages + (file->file->last), msg, 
                sizeof(mon_message) + len);
        file->file->last += sizeof(mon_message) + len;
        msync(file->file, sizeof(file->file), MS_SYNC);
        r = pthread_mutex_unlock(&file->file->mutex);
        if(r == -1) return -1;
    }
    else {
        if(msgflag == 0){
            // bloquant 
            int r = pthread_mutex_lock(&file->file->mutex);
            // if(r == -1) return -1;
            while(m_nb(file) >= file->file->capacite){
                pthread_cond_wait(&file->file->env, &file->file->mutex);
            }
            r = pthread_mutex_unlock(&file->file->mutex);
            // if(r == -1) return -1;
            return m_envoi(file, msg, len, msgflag);
        }
        else {
            errno = EAGAIN;
            return -1;
        }
    }

    // signaux a envoyer SSI aucun proc suspendu en attente de ce message
    size_t l = m_size_signal(file);
    for(int i=0; i < m_nbSignal(file); i++){
     //pour chaque signalEnregistre
        char buf[l];
        for (int j=0; j < l; j++){
            buf[j] = file->file->enregistrement[j + i*l];
        }
        signalEnregis * sig = (signalEnregis*)buf;
        mon_message * mess = (mon_message*)msg;
        if(sig->typeMess == mess->type){
            // meme type envoyer signal 
            // verif aucun en attente 
            size_t l = sizeof(je_suis_bloque);
            for(int i=0; i < m_nbType(file); i++){
                char buf[l];
                for(int j=0; j < l ; j++){
                    buf[j] = file->file->bloque[j + i*l];
                }
                je_suis_bloque *b = (je_suis_bloque*)buf;
                //verif type 
                if(b->typeMess == sig->typeMess){
                    if(b->cb == 0){
                        kill(sig->pid, sig->typeSignal);
                        // desenregistre 
                        desenregistrement(file, sig->pid);
                    }else{
                        printf("impossible processus en attente pour ce type de message\n");
                    }
                }
                
            }
            kill(sig->pid, sig->typeSignal);
            // desenregistre 
            desenregistrement(file, sig->pid);
        }
    }
    pthread_cond_broadcast(&file->file->rec);
    return 0;
}

//supprimer element a la position i et decaler les autres
void suppressionMess(MESSAGE *file, size_t taille, size_t deb){
    int r = pthread_mutex_lock(&file->file->mutex);
    if(r == -1) exit(0);
    for(int i=deb; i < file->file->last; i++){
        //pour chaque mon_message
        file->file->messages[i-taille] = file->file->messages[i];      
    }
    file->file->last -= taille;
    msync(file->file, sizeof(file->file), MS_SYNC);
    r = pthread_mutex_unlock(&file->file->mutex);
}


// lire message 
ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags, int tour){
    if(!(file->type & S_IRUSR)) return -1;
    // len pas assez grand et file non vide
    if(len >= m_message_len(file) ){
        size_t l = m_size_messages(file);
        if(type == 0 && m_nb(file) > 0){
            char buf[l];
            for (int j=0;j<l;j++){
                buf[j] = file->file->messages[j];
            }
            mon_message * mess = (mon_message*)buf;
            memcpy(msg, mess, sizeof(mon_message) + mess->len);
            suppressionMess(file, m_message_taille(mess), m_message_taille(mess));
            pthread_cond_broadcast(&file->file->env);
            return (mess->len);
        }else if(type > 0){
            // lire le premier message dont le type est type
            size_t emplacement = 0;
            for(int i=0; i < m_nb(file); i++){
                char buf[l];
                for(int j=0; j < l; j++){
                    buf[j] = file->file->messages[j + emplacement];
                }
                mon_message * mess = (mon_message*)buf;
                emplacement += sizeof(mon_message) + mess->len;
                if(mess->type == type){
                    memcpy(msg, mess, sizeof(mon_message) + mess->len);
                    suppressionMess(file, m_message_taille(mess), emplacement);
                    if(tour == 1){
                        size_t l= sizeof(je_suis_bloque);
                        for(int i=0; i < m_nbType(file); i++){
                            char buf[l];
                            for(int j=0; j< l ; j++){
                                buf[j] = file->file->bloque[j + i*l];
                            }
                            je_suis_bloque *b= (je_suis_bloque*)buf;
                            if(b->typeMess == type){
                                int r = pthread_mutex_lock(&file->file->mutex);
                                b->cb--;
                                msync(file->file, sizeof(file->file), MS_SYNC);
                                r = pthread_mutex_unlock(&file->file->mutex);
                                if(r == -1) return -1;
                                pthread_cond_broadcast(&file->file->env);
                                return (mess->len);
                            }
                        }
                    }
                    pthread_cond_broadcast(&file->file->env);
                    return (mess->len);
                }      
            }
        }else if(type < 0){
            // lire le premier message dont type inferier ou egal a |type|
            size_t emplacement = 0;
            for(int i=0; i < m_nb(file); i++){
                char buf[l];
                for(int j=0; j < l; j++){
                    buf[j] = file->file->messages[j + emplacement];
                }
                mon_message * mess = (mon_message*)buf;
                emplacement += sizeof(mon_message) + mess->len;
                if(mess->type  <= labs(type)){
                    memcpy(msg, mess, sizeof(mon_message) + mess->len);
                    suppressionMess(file, m_message_taille(mess), emplacement);
                    pthread_cond_broadcast(&file->file->env);
                    return (mess->len);
                }       
            }
        }
        //bloquant 
        //si j'arrive c'est que jai pas le type de ce message 
        if(flags == 0){
            int r = pthread_mutex_lock(&file->file->mutex);
            // if(r == -1) return -1;
            while(m_nb(file) == 0){
                pthread_cond_wait(&file->file->rec, &file->file->mutex);
            }
            r = pthread_mutex_unlock(&file->file->mutex);
            // if(r == -1) return -1;
            if(type>0){
                //je dois le mettre dans mon tableau
                size_t l= sizeof(je_suis_bloque);
                //regarder si il y a deja un un attente (donc on fait juste ++)
                for(int i=0; i < m_nbType(file); i++){
                    char buf[l];
                    for(int j=0; j < l ; j++){
                        buf[j] = file->file->bloque[j + i*l];
                    }
                    je_suis_bloque *b = (je_suis_bloque*)buf;
                    //on a trouver 
                    if(b->typeMess == type){
                        //si tour =0 on a pas deja fait de tour donc ++
                        if(tour == 0){
                            int r = pthread_mutex_lock(&file->file->mutex);
                            if(r == -1) return -1;
                            b->cb += 1;
                            msync(file->file, sizeof(file->file), MS_SYNC);
                            r = pthread_mutex_unlock(&file->file->mutex);
                            if(r == -1) return -1;
                        }
                        //on a deja fait un tour donc fait rien juste rappel 
                        return (m_reception(file,msg,len,type,flags,1));
                    }
                }
                //cree 
                int r = pthread_mutex_lock(&file->file->mutex);
                if(r == -1) return -1;
                je_suis_bloque *b = malloc(sizeof(je_suis_bloque));
                b->cb = 1;
                b->typeMess = type;
                memcpy(file->file->bloque + (file->file->lastBloque), b, sizeof(je_suis_bloque));
                file->file->lastBloque += sizeof(je_suis_bloque);
                msync(file->file, sizeof(file->file), MS_SYNC);
                r = pthread_mutex_unlock(&file->file->mutex);
                if(r == -1) return -1;
                return m_reception(file, msg, len, type, flags, 1);
            }
            return m_reception(file, msg, len, type, flags, 0);
        }
        else {
            errno = EAGAIN;
            return -1;
        }    
    }
    errno = EMSGSIZE;
    return -1;
}

int enregistrement(MESSAGE *file, int signal, long type){
    if(m_nbSignal(file) < NBSIG){
        size_t l = sizeof(signalEnregis);
        // verifier que proc pas deja enregistrer
        for(int i=0; i < m_nbSignal(file); i++){
            //pour chaque signalEnregis
            char buf[l];
            for (int j=0; j < l; j++){
                buf[j] = file->file->enregistrement[j + i*l];
            }
            signalEnregis * sig = (signalEnregis*)buf;
            if(sig->pid == getpid()){
                return -1;
            }
        }

        signalEnregis *sig = malloc(sizeof(signalEnregis));
        sig->pid = getpid();
        sig->typeMess = type;
        sig->typeSignal = signal;
        int r = pthread_mutex_lock(&file->file->mutex);
        if(r == -1) return -1;
        memcpy(file->file->enregistrement + (file->file->lastSignal), 
                    sig, sizeof(signalEnregis));
        file->file->lastSignal += sizeof(signalEnregis);
        msync(file->file, sizeof(file->file), MS_SYNC);
        r = pthread_mutex_unlock(&file->file->mutex);
        if(r == -1) return -1;
    }
    else {
        errno = EAGAIN;
        return -1;
    }
    return 0;
}

//supprimer element a la position i et decaler les autres
void suppressionSig(MESSAGE *file, int pos){
    int r = pthread_mutex_lock(&file->file->mutex);
    if(r == -1) exit(0);
    for(int i=pos; i < m_nbSignal(file); i++){
        //pour chaque signalEnregis
        size_t l = sizeof(signalEnregis);
        for (int j=0; j < l; j++){
            file->file->enregistrement[j + i*l] = 
                    file->file->enregistrement[j + (i+1)*l];
        }
    }
    file->file->lastSignal -= sizeof(signalEnregis);
    msync(file->file, sizeof(file->file), MS_SYNC);
    r = pthread_mutex_unlock(&file->file->mutex);
}

int desenregistrement(MESSAGE *file, pid_t pid){
    size_t l = sizeof(signalEnregis);
    //lire le premier processus dont le pid est getpid()
    for(int i=0; i < m_nbSignal(file); i++){
        //pour chaque signalEnregis
        char buf[l];
        for (int j=0; j < l; j++){
            buf[j] = file->file->enregistrement[j + i*l];
        }
        signalEnregis * sig = (signalEnregis*)buf;
        if(sig->pid == pid){
            suppressionSig(file, i);
            return 0;
        }
    }
    errno = EAGAIN;
    return -1;
}

// affiche dun MESAGGE
void affichage_message(MESSAGE *m){
    if(m == NULL) printf("null la\n");
    else{
        printf("\n-----DEBUT-----\n");
        printf("type : %ld\n",m->type);
        affichage_entete(m->file, m_nb(m), m_size_messages(m), m_nbSignal(m),m_nbType(m));
    } 
}
// affichage dune entete
void affichage_entete(enteteFile *e, size_t nbM, size_t lm, size_t nbS, size_t nbT){
    printf("long max : %zu\n",e->longMax);
    printf("capacite : %zu\n", e->capacite);
    printf("first : %d, last : %d \n", e->first, e->last);
    printf("nbMess : %zu\n",nbM);
    printf("nbSig : %zu\n",nbS);
    printf("nbBloque : %zu\n",nbT);
    printf("\n");
    size_t ls = sizeof(signalEnregis);
    for(int i=0; i < nbS; i++){
        char buf[ls];
        for (int j=0; j < ls; j++){
            buf[j] = e->enregistrement[j+ i*ls];
        }
        signalEnregis * sig = (signalEnregis*)buf;
        printf("pid : %zu, typeMess : %zu, typeSignal : %u\n", 
                sig->pid, sig->typeMess, sig->typeSignal);
    }
    printf("\n");

    size_t ls2 = sizeof(je_suis_bloque);
    for(int i=0; i < nbT; i++){
        char buf[ls2];
        for (int j=0; j < ls2; j++){
            buf[j] = e->bloque[j+ i*ls2];
        }
        je_suis_bloque * b = (je_suis_bloque*)buf;
        printf("typemess : %zu, cbBloque : %zu\n", 
                b->typeMess, b->cb);
    }
    size_t emplacement = 0;
    for(int i=0; i < nbM; i++){
        char buf[lm];
        for(int j=0; j < lm; j++){
            buf[j] = e->messages[j + emplacement];
        }
        mon_message * mess = (mon_message*)buf;
        mon_message * message = malloc(m_message_taille(mess));
        memcpy(message, mess, m_message_taille(mess));
        affichage_mon_message(message); 
        emplacement += m_message_taille(mess);       
    }
    printf("-----FIN-----\n\n");
}
// affichage dun message
void affichage_mon_message(mon_message *m){
    printf("Message -> type : %zu, len : %zu, mess : %s\n",
        m->type, m->len, m->mtext);

}
