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
    char *txt="*notification* Message arrivee !\n";
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
    pthread_condattr_setpshared(&condattr,PTHREAD_PROCESS_SHARED);
    if( ( code = pthread_cond_init(pcond, &condattr) ) != 0)
      return code;
    code = pthread_cond_init(pcond, &condattr)  ;
    return code;
}

// t taille 
size_t m_message_len(MESSAGE *file){
    return file->file->longMax;
}
size_t m_capacite(MESSAGE *file){
    return file->file->capacite;
}
size_t m_nb(MESSAGE *file){
    return (file->file->last)/m_size_messages(file);
}
size_t m_nbSignal(MESSAGE *file){
    return (file->file->lastSignal)/sizeof(signalEnregis);
}

size_t m_size_messages(MESSAGE *file){
    return (sizeof(mon_message)+file->file->longMax);
}
size_t m_size_signal(MESSAGE *file){
    return(sizeof(signalEnregis));
}

// connexion a une file de message ou creation
MESSAGE *m_connexion(const char *nom, int options, int nb, ...){ 
    //size_t nb_msg, size_t len_max, mode_t mode
    // nb = 0 ou 3 -> nb darguments en plus
    MESSAGE *mess=malloc(sizeof(MESSAGE));
    enteteFile *entete;
    //option ne contient pas ocreat
    if((options == O_RDONLY)||(options == O_WRONLY)||(options == O_RDWR)){
        // on ne cree pas la file on a que 2 arg 
        int d = shm_open(nom, options, S_IRUSR | S_IWUSR);
        if(d<0) return NULL;
        struct stat buf;
        int r =fstat(d,&buf);
        if(r== -1) return NULL;
        entete=mmap(NULL,buf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,d,0);
        mess->type=buf.st_mode;
        mess->file=entete;        
    }else{
        va_list param;
        va_start(param, nb);
        size_t nb_msg;
        size_t len_max;
        mode_t mode;
        nb_msg = (size_t) va_arg(param, size_t);
        len_max = (size_t) va_arg(param, size_t);
        mode = (mode_t)va_arg(param, int);
        size_t tailleent = sizeof(enteteFile) + (sizeof(mon_message)+len_max)*nb_msg
                    +sizeof(signalEnregis)*NBSIG;
        if(nom == NULL){
            //file ano
            entete = mmap(NULL, tailleent, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,-1,0);
        }else {
            int d=shm_open(nom, options, mode);
            ftruncate(d,sizeof(enteteFile));
            if(d<0) return NULL;
            entete = mmap(NULL, tailleent, PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
        }
        int r=initialiser_mutex(&entete->mutex);
        if(r ==-1) return NULL;
        r=pthread_mutex_lock(&entete->mutex);
        if(r == -1) return NULL;
        entete->capacite = nb_msg;
        entete->first = 0;
        entete->last = 0;
        entete->lastSignal = 0;
        entete->longMax = len_max;
        mess->type = mode;
        mess->file = entete;
        msync(&mess->file, sizeof(mess->file), MS_SYNC);
        r=pthread_mutex_unlock(&entete->mutex);
        if(r == -1) return NULL;
        va_end(param);
    }
    return mess;   
}

// deconnection de la file 
int m_deconnexion(MESSAGE *file){
    if(munmap(file->file, 
        sizeof(enteteFile)+ m_size_messages(file) *file->file->capacite 
            + sizeof(signalEnregis)*NBSIG)==-1)
        return -1;    
    return 0;
}

//destruction de la file
int m_destruction(const char *nom){
    if(shm_unlink(nom)==-1) return -1;
    return 0;
}

// envoie de message
int m_envoi(MESSAGE *file, const void *msg, 
    size_t len, int msgflag){
    // tableau de message non plein
    if(m_nb(file) < file->file->capacite){
        // verifier len pas trop grand
        if(file->file->longMax < len) return -1;
        int r = pthread_mutex_lock(&file->file->mutex);
        if(r == -1) return -1;
        memcpy(file->file->messages+(file->file->last),msg,sizeof(mon_message)+len);
        file->file->last += m_size_messages(file);
        msync(file->file, sizeof(file->file), MS_SYNC);
        r = pthread_mutex_unlock(&file->file->mutex);
        if(r == -1) return -1;
    }
    else {
        if(msgflag == 0){
            // bloquant 
            while(m_nb(file)>= file->file->capacite){
                sleep(5);
            }
            return m_envoi(file,msg,len,msgflag);
        }
        else {
            errno = EAGAIN;
            return -1;
        }
    }

    // signaux a envoyer SSI aucun proc suspendu en attente de ce message
    size_t l = m_size_signal(file);
    for(int i=0;i<m_nbSignal(file); i++){
     //pour chaque signalEnregistre
        char buf[l];
            for (int j=0;j<l;j++){
                buf[j] = file->file->enregistrement[j+ i*l];
            }
            signalEnregis * sig = (signalEnregis*)buf;
            mon_message * mess = (mon_message*)msg;
            if(sig->typeMess==mess->type){
                //meme type envoyer signal 
                kill(sig->pid,sig->typeSignal);
                //desenregistre 
                desenregistrement(file);
            }
    }
    return 0;
}

//supprimer element a la position i et decaler les autres
void suppressionMess(MESSAGE *file, int pos){
    int r = pthread_mutex_lock(&file->file->mutex);
    if(r == -1) exit(0);
    for(int i=pos;i<m_nb(file); i++){
        //pour chaque mon_message
        size_t l = m_size_messages(file);
        for (int j=0;j<l;j++){
            file->file->messages[j+i*l]= file->file->messages[j+(i+1)*l];
        }
    }
    file->file->last-=m_size_messages(file);
    msync(file->file, sizeof(file->file), MS_SYNC);
    r = pthread_mutex_unlock(&file->file->mutex);
}

// lire message 
ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags){
    if(len < m_message_len(file)){
        errno = EMSGSIZE;
        return -1;
    }else if(m_nb(file)==0){
        if(flags == 0){
            // bloquant
            while(m_nb(file)==0){
                sleep(5);;
            }
            return m_reception(file,msg,len,type,flags);
        }
        else {
            errno = EAGAIN;
            return -1;
        }
    }else{
        size_t l = m_size_messages(file);
        if(type== 0){
            char buf[l];
            for (int j=0;j<l;j++){
                buf[j] = file->file->messages[j];
            }
            mon_message * mess = (mon_message*)buf;
            memcpy(msg,mess, m_size_messages(file));
            suppressionMess(file,0);
            return (mess->len);
        }else if(type>0){
            // lire le premier message dont le type est type
            for(int i=0;i<m_nb(file); i++){
                // pour chaque mon_message
                char buf[l];
                for (int j=0;j<l;j++){
                    buf[j] = file->file->messages[j+ i*l];
                }
                mon_message * mess = (mon_message*)buf;
                if(mess->type==type){
                    memcpy(msg,mess, m_size_messages(file));
                    suppressionMess(file,i);
                    return (mess->len);
                }
            }
            errno = EAGAIN;
            return -1;
        }else if(type<0){
            // lire le premier message dont type inferier ou egal a |type|
            for(int i=0;i<m_nb(file); i++){
                // pour chaque mon_message
                char buf[l];
                for (int j=0;j<l;j++){
                    buf[j] = file->file->messages[j+ i*l];
                }
                mon_message * mess = (mon_message*)buf;
                if(mess->type<=labs(type)){
                    memcpy(msg,mess, m_size_messages(file));
                    suppressionMess(file,i);
                    return (mess->len);
                } 
            }
            errno = EAGAIN;
            return -1;
        }    
    }
    return -1;
}

int enregistrement(MESSAGE *file, int signal, long type){
    if(m_nbSignal(file) < NBSIG){
        signalEnregis *sig = malloc(sizeof(signalEnregis));
        sig->pid = getpid();
        sig->typeMess = type;
        sig->typeSignal = signal;
        int r = pthread_mutex_lock(&file->file->mutex);
        if(r == -1) return -1;
        memcpy(file->file->enregistrement+(file->file->lastSignal),sig,sizeof(signalEnregis));
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
    for(int i=pos;i<m_nbSignal(file); i++){
        //pour chaque signalEnregis
        size_t l = sizeof(signalEnregis);
        for (int j=0;j<l;j++){
            file->file->enregistrement[j+i*l]= file->file->enregistrement[j+(i+1)*l];
        }
    }
    file->file->lastSignal -= sizeof(signalEnregis);
    msync(file->file, sizeof(file->file), MS_SYNC);
    r = pthread_mutex_unlock(&file->file->mutex);
}

int desenregistrement(MESSAGE *file){
    size_t l = sizeof(signalEnregis);
    //lire le premier processus dont le pid est getpid()
    for(int i=0;i<m_nbSignal(file); i++){
        //pour chaque signalEnregis
        char buf[l];
        for (int j=0;j<l;j++){
            buf[j] = file->file->enregistrement[j+ i*l];
        }
        signalEnregis * sig = (signalEnregis*)buf;
        if(sig->pid == getpid()){
            suppressionSig(file,i);
            return 0;
        }
    }
    errno = EAGAIN;
    return -1;
}

// affiche dun MESAGGE
void affichage_message(MESSAGE *m){
    if(m==NULL) printf("null la\n");
    else{
        printf("\n-----DEBUT-----\n");
        printf("type : %ld\n",m->type);
        affichage_entete(m->file,m_nb(m),m_size_messages(m), m_nbSignal(m));
    } 
}
// affichage dune entete
void affichage_entete(enteteFile *e, size_t nbM, size_t lm, size_t nbS){
    printf("long max : %zu\n",e->longMax);
    printf("capacite : %zu\n", e->capacite);
    printf("first : %d, last : %d \n", e->first, e->last);
    printf("nbMess : %zu\n",nbM);
    printf("nbSig : %zu\n",nbS);
    printf("\n");
    size_t ls = sizeof(signalEnregis);
    for(int i=0;i<nbS;i++){
        char buf[ls];
        for (int j=0;j<ls;j++){
            buf[j] = e->enregistrement[j+ i*ls];
        }
        signalEnregis * sig = (signalEnregis*)buf;
        printf("pid : %zu, typeMess : %zu, typeSignal : %u\n",sig->pid,sig->typeMess, sig->typeSignal);
    }

    for(int i=0;i<nbM;i++){
        char buf[lm];
        for (int j=0;j<lm;j++){
            buf[j] = e->messages[j+ i*lm];
        }
        mon_message * mess = (mon_message*)buf;
        affichage_mon_message(mess); 
    }
    printf("-----FIN-----\n");
}
// affichage dun message
void affichage_mon_message(mon_message *m){
    printf("Message -> type : %zu, len : %zu, mess : %s\n",
        m->type, m->len, m->mtext);

}
