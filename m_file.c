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

// t taille 
size_t m_message_len(MESSAGE *file){
    return(file->file->longMax);
}
size_t m_capacite(MESSAGE *file){
    return(file->file->capacite);
}
size_t m_nb(MESSAGE *file){
    return(file->file->first +1);
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
        nb_msg = (size_t) va_arg(param, size_t);
        len_max = (size_t) va_arg(param, size_t);
        mode = (mode_t)va_arg(param, int); 
        m->type = mode;
    }
    if(nom == NULL){
        //file ano caca
        enteteFile *mem=malloc(sizeof(enteteFile)); //pointeur vers region de memoire partagee
        mem = mmap(NULL, sizeof(enteteFile), PROT_READ| PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,-1,0);
        int r =  initialiser_mutex(&mem->mutex);
            if(r == -1) return NULL;
            r = pthread_mutex_lock(&mem->mutex);
        if(r == -1) return NULL;
        mem->nb_proc_co += 1;
        mem->nb_proc_co = 1;
        mem->capacite = nb_msg;
        mem->longMax = len_max;
        msync(mem, sizeof(mem), MS_SYNC);
        r = pthread_mutex_unlock(&mem->mutex);
        if(r == -1) return NULL;
        m->file= mem;
    
    }else{
        //ouvrir morceau de memoire partagee 
        if((options == O_RDONLY)||(options == O_WRONLY)||(options == O_RDWR)){
            // on ne cree pas la file on a que 2 arg 
            int d = shm_open(nom, options,S_IRUSR | S_IWUSR);
            ftruncate(d,sizeof(enteteFile));
            // printf("coucou 1\n");
            if(d<0) return NULL;
            enteteFile *mem;
            struct stat buf;
            fstat(d,&buf);
            // printf("coucou 2\n");
            m->type = buf.st_mode;
            mem = mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
            // printf("coucou 3\n");
            int r = pthread_mutex_lock(&mem->mutex);
            // printf("r=%d\n",r);
            if(r == -1) return NULL;
            // printf("coucou 4\n");
            mem->nb_proc_co+=1;
            r = pthread_mutex_unlock(&mem->mutex);
            if(r == -1) return NULL;
            // printf("coucou 5\n");
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
            mem=mmap(NULL,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
            int r =  initialiser_mutex(&mem->mutex);
            if(r == -1) return NULL;
            // int r = pthread_mutex_lock(&mem->mutex);
            // if(r == -1) return NULL;
            mem->nb_proc_co = 1;
            mem->capacite = nb_msg;
            mem->longMax = len_max;
            // msync(mem, sizeof(mem), MS_SYNC);
            // r = pthread_mutex_unlock(&mem->mutex);
            // if(r == -1) return NULL;
            m->file = mem;
            // munmap(mem,sizeof(enteteFile));
            
        }else if((options == (O_RDONLY| O_CREAT| O_EXCL))||
            (options == (O_WRONLY|O_CREAT|O_EXCL))||
            (options == (O_RDWR|O_CREAT|O_EXCL))){

            //si la file existe deja il faut echouer normalement shm_open le fait
            int d = shm_open(nom, options, mode);
            if(d<0) return NULL;
            ftruncate(d,sizeof(enteteFile));
            enteteFile *mem;
            mem=mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
            // int r =  initialiser_mutex(&mem->mutex);
            // if(r == -1) return NULL;
            int r = pthread_mutex_lock(&mem->mutex);
            if(r == -1) return NULL;
            mem->capacite = nb_msg;
            mem->longMax = len_max;
            mem->nb_proc_co = 1;
            msync(mem, sizeof(mem), MS_SYNC);
            r = pthread_mutex_unlock(&mem->mutex);
            if(r == -1) return NULL;
            m->file = mem;
            // munmap(mem,sizeof(enteteFile));
        }
    }
    va_end(param);
    return m;
}


// deconnection de la file A VOIR POUR MUNMAP
int m_deconnexion(MESSAGE *file){
    if(file->file->nb_proc_co>0){
        file->file->nb_proc_co-=1;
        file->file=NULL;
        file->type=0;
        // if(munmap(mem, sizeof(enteteFile))==-1){
            //     return -1;
            // }
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
            if(shm_unlink(nom)==-1){
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
    if(msgflag == 0){

    }
    else if(msgflag==O_NONBLOCK){
        if(file->file->last == file->file->capacite){
            errno = EAGAIN;
            return -1;
        }
        else {
            // printf("coucou else\n");

            // verifier len pas trop grand
            if(file->file->longMax < len) return -1;

            int r = pthread_mutex_lock(&file->file->mutex);
            if(r == -1) return -1;
            // printf("coucou1\n");
            mon_message *m = malloc(sizeof(msg)+len);
            // printf("coucou2\n");
            memmove(m,msg,sizeof(mon_message)+len);
            // printf("coucou3\n");
            affichage_mon_mess(m);
            printf("\n");
            memmove(file->file->tabMessage+(file->file->last),m,sizeof(mon_message)+len);
            // file->file->tabMessage[file->file->last] =  *m;
            // printf("coucou4\n");
            file->file->last++;
            msync(file->file, sizeof(file->file), MS_SYNC);
            // printf("coucou5\n");
            r = pthread_mutex_unlock(&file->file->mutex);
            if(r == -1) return -1;
        }
    }
    else return -1;
    // printf("coucoufin\n");
    return 0;
}


void suppression(MESSAGE *file, int pos){
    for(int i=pos;i<m_nb(file)-1; i++){
        file->file->tabMessage[i]=file->file->tabMessage[i+1];
    }
}

// lire message 
ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags){
    if(flags==O_NONBLOCK){
        //sizeof == taille en octet
        mon_message msgvoulu; 
        if(len< sizeof(msgvoulu)){
            errno = EMSGSIZE;
            return -1;
        }else if(m_nb(file)==0){
            errno = EAGAIN;
            return -1;
        }else{
            if(type== 0){
                //lire le premier message dans la file
                msgvoulu=file->file->tabMessage[file->file->first];
                suppression(file,file->file->first);
                return(sizeof(msgvoulu));
            }else if(type>0){
                //lire le premier message dont le type est type
                for(int i=0; i<m_nb(file);i++){
                    mon_message msgtmp=file->file->tabMessage[i];
                    if(type== msgtmp.type){
                        //on est ok
                        msgvoulu=msgtmp;
                        memmove(msg,&msgvoulu,sizeof(msgvoulu));
                        suppression(file,i);
                        return sizeof(msgvoulu);
                    }
                }
                errno = EAGAIN;
                return -1;
            }else if(type<0){
                //lire le premier message dont type inferier ou egal a |type|
                for(int i=0; i<m_nb(file);i++){
                    mon_message msgtmp=file->file->tabMessage[i];
                    if(msgtmp.type<=type){
                        //on est ok
                        msgvoulu=msgtmp;
                        memmove(msg,&msgvoulu,sizeof(msgvoulu));
                        suppression(file, i);
                        return sizeof(msgvoulu);
                    }
                }
                errno = EAGAIN;
                return -1;
            }
        }
    }else{
        //on est dans le cas de la lecture bloquante

    }
    return -1;
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
            affichage_mon_mess(&e->tabMessage[i]);
        }
    }
}
void affichage_mon_mess(mon_message *mm){
    printf("type mon mess : %ld %s", mm->type,mm->mtext);
}


int main(int argc, char const *argv[]){
    // printf("hey\n");
    // MESSAGE * m = m_connexion("/titi", O_RDWR|O_CREAT, 3, 3, 10, S_IRUSR | S_IWUSR);
    // affichage_message(m);
    // printf("\n");
    // MESSAGE *m1 = m_connexion("/titi",O_RDWR, 0);
    // affichage_message(m1);
    // printf("%d\n",m_deconnexion(m1));
    // affichage_message(m);
    // affichage_message(m1);

    //Decommenter pour voir si deconnexion marche bien
    // printf("%d\n",m_deconnexion(m));
    // printf("destruc %d\n",m_destruction("/titi"));
    // MESSAGE *m2 = m_connexion("/titi",O_RDWR, 0);
    // affichage_message(m2);


    char * path = "/a";

    MESSAGE* m = m_connexion(path, O_RDWR|O_CREAT|O_EXCL, 3, 3, 10, S_IRUSR | S_IWUSR);
    
    // MESSAGE *m = m_connexion(path,O_RDWR, 0);
    affichage_message(m);
    printf("\n");
    MESSAGE *m1 = m_connexion(path,O_RDWR, 0);
    affichage_message(m1);
    printf("\n\n");

    // int t[2] = {-12, 99};
    char *t = "salut1";
    // printf("test1\n");
    // //char * ti = "coucou";
    mon_message *mes = malloc(sizeof(mon_message) + sizeof(t)+1);
    // printf("test2\n");
    if( mes == NULL ) return -1;
    // printf("test3\n");
    mes->type = (long) getpid(); /* comme type de message, on choisit l’identité
    //                             * de l’expéditeur */
    // printf("test4\n");
    memmove( mes->mtext, t, sizeof(t)+1) ; /* copier les deux int à envoyer */
    // printf("test5\n");
    affichage_mon_mess(mes);
    printf("\n");

    for(int j=0;j<4;j++){
        int i = m_envoi(m,mes,sizeof(t)+1,O_NONBLOCK);
        if(i == 0){
            printf("Ok %d\n",j);
            affichage_message(m);
            printf("\n");
            affichage_message(m1);
            printf("\n");
        }
        else if(i == -1 && errno == EAGAIN){
            printf("file pleine, attendez un peu\n");
        }
        else {
            printf("erreur\n");
        }

    }
    
    

    // sleep(15);
    printf("\ndeco %d\n",m_deconnexion(m));
    affichage_message(m);
    affichage_message(m1);
    printf("destruc %d\n",m_destruction(path));
    printf("deco %d\n",m_deconnexion(m1));
    printf("destruc %d\n",m_destruction(path));
    // MESSAGE *m2 = m_connexion(path,O_RDWR, 0);
    // affichage_message(m2);
    affichage_message(m);
    affichage_message(m1);


    return 0;



    return 0;
}
