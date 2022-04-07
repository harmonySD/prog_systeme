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
#include <stdarg.h>

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
MESSAGE *m_connexion(const char *nom, int options, int nb, ...){ //size_t nb_msg, size_t len_max, mode_t mode
        // nb = 0 ou 3 -> nb darguments en plus
        MESSAGE *m=malloc(sizeof(MESSAGE));
        va_list param;
        va_start(param, nb);
        size_t nb_msg;
        size_t len_max;
        mode_t mode;
        if(nb!=0){
            nb_msg =(size_t) va_arg(param, size_t);
            len_max =(size_t) va_arg(param, size_t);
            mode = (mode_t)va_arg(param, int); 
            m->type=mode;
        }
        if(nom == NULL){
            //file ano caca
            enteteFile *mem=malloc(sizeof(enteteFile)); //pointeur vers region de memoire partagee
            mem = mmap(NULL, sizeof(enteteFile), PROT_READ| PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1,0);
            int r = pthread_mutex_lock(&(mem->mutex));
            mem->nb_proc_co+=1;
            mem->nb_proc_co=1;
            mem->capacite=nb_msg;
            mem->longMax=len_max;
            msync(mem, sizeof(mem), MS_SYNC);
            r = pthread_mutex_unlock(&mem->mutex);
            m->file= mem;
       
        }else{
            //ouvrir morceau de memoire partagee 
            if((options == O_RDONLY)||(options == O_WRONLY)||(options == O_RDWR)){
                // on ne cree pas la file on a que 2 arg 
                int d = shm_open(nom, options);
                ftruncate(d,sizeof(enteteFile));
                if(d<0) return NULL;
                enteteFile *mem;
                struct stat buf;
                fstat(d,&buf);
                m->type= buf.st_mode;
                mem=mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
                int r = pthread_mutex_lock(&(mem->mutex));
                mem->nb_proc_co+=1;
                r = pthread_mutex_unlock(&mem->mutex);
                m->file=mem;
                
                //mettre dans mem l'entete deja prensete dans le memoire partager
                //entetefile existe deja ? on connait les file existante ? oui juste a ouvrir mmap (nom)
            }else if((options == (O_RDONLY| O_CREAT))||(options == (O_WRONLY|O_CREAT))||
                (options == (O_RDWR|O_CREAT))){
                //on cree la file 
                // printf("mode %d\n",mode);
                // printf("%d\n",S_IRUSR | S_IWUSR);
                int d = shm_open(nom, options, mode);
                ftruncate(d,sizeof(enteteFile));
                if(d<0) return NULL;
                enteteFile *mem;
                mem=mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
                int r = pthread_mutex_lock(&(mem->mutex));
                mem->nb_proc_co=1;
                mem->capacite=nb_msg;
                mem->longMax=len_max;
                msync(mem, sizeof(mem), MS_SYNC);
                r = pthread_mutex_unlock(&mem->mutex);
                m->file=mem;
                
            }else if((options == (O_RDONLY| O_CREAT| O_EXCL))||
                (options == (O_WRONLY|O_CREAT|O_EXCL))||
                (options == (O_RDWR|O_CREAT|O_EXCL))){

                //si la file existe deja il faut echouer normalement shm_open le fait
                int d = shm_open(nom, options, mode);
                if(d<0) return NULL;
                ftruncate(d,sizeof(enteteFile));
                enteteFile *mem;
                mem=mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
                int r = pthread_mutex_lock(&mem->mutex);
                mem->capacite=nb_msg;
                // gerer erreur
                mem->longMax=len_max;
                mem->nb_proc_co=1;
                r = pthread_mutex_unlock(&mem->mutex);
                m->file=mem;
               
                

            }
    
        }
        va_end(param);
        return m;
}

// deconnection de la file 
int m_deconnexion(MESSAGE *file){
    if(file->file->nb_proc_co>0){
        file->file->nb_proc_co-=1;
        file->file=NULL;
        file->type=0;
        return 0;
    }
    return -1;
}

// destruction de la file
int m_destruction(const char *nom){
    int d=shm_open(nom,O_RDWR, 0666);
    ftruncate(d,sizeof(enteteFile));
    enteteFile *mem=mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
    if(mem->nb_proc_co<=0){
            if(munmap(mem, sizeof(enteteFile))==-1){
                return -1;
            }
            return 0;
    }else{
        return -1;
    }

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
void affichage_message(MESSAGE *m){
    if(m==NULL) printf("null\n");
    else if(m->file==NULL && m->type==0){
        printf("null\n");
    }else{
        printf("type : %ld\n",m->type);
        affichage_entete(m->file);
    }
    
}
void affichage_entete(enteteFile *e){
    if(e==NULL){
        printf("null\n");
    }else{
        printf("long max : %zu\n",e->longMax);
        printf("capacite : %zu\n", e->capacite);
        printf("first : %d last : %d \n", e->first, e->last);
        printf("nb_proc :%d\n",e->nb_proc_co);
        for(int i= e->first; i<e->last; i++){
            affichage_mon_mess(e->tabMessage[i]);
        }
    }
}
void affichage_mon_mess(mon_message mm){
    printf("type mon mess : %ld %s", mm.type, mm.mtext);
}


int main(int argc, char const *argv[]){
    MESSAGE * m = m_connexion("/toto", O_RDWR|O_CREAT, 3, 3, 10, S_IRUSR | S_IWUSR);
    affichage_message(m);
    printf("\n");
    MESSAGE *m1 = m_connexion("/toto",O_RDWR, 0);
    affichage_message(m1);
    printf("%d\n",m_deconnexion(m1));
    affichage_message(m);
    affichage_message(m1);
    printf("destruc %d\n",m_destruction("/toto"));
    printf("%d\n",m_deconnexion(m));
    printf("destruc %d\n",m_destruction("/toto"));
    affichage_message(m);
    affichage_message(m1);
    return 0;
}
