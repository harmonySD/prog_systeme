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
    return file->file->last;
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
        //
        mon_message *messg=(mon_message *)malloc(sizeof(mon_message)+len_max+sizeof(long));
        mem->tabMessage=malloc(sizeof(messg)*nb_msg);
        //
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
            if(d<0) return NULL;
            enteteFile *mem;
            struct stat buf;
            fstat(d,&buf);
            m->type = buf.st_mode;
            mem = mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
            int r = pthread_mutex_lock(&mem->mutex);
            if(r == -1) return NULL;
            mem->nb_proc_co+=1;
            r = pthread_mutex_unlock(&mem->mutex);
            if(r == -1) return NULL;
            m->file=mem; 
        //mettre dans mem l'entete deja prensete dans le memoire partager
        //entetefile existe deja ? on connait les file existante ? oui juste a ouvrir mmap (nom)
        }else if((options == (O_RDONLY| O_CREAT))||(options == (O_WRONLY|O_CREAT))||
            (options == (O_RDWR|O_CREAT))){
            //on cree la file
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
             //
            mon_message *messg=(mon_message *)malloc(sizeof(mon_message)+len_max+sizeof(long));
            mem->tabMessage=malloc(sizeof(messg)*nb_msg);
            //

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

            //
            mon_message *messg=(mon_message *)malloc(sizeof(mon_message)+len_max+sizeof(long));
            mem->tabMessage=malloc(sizeof(messg)*nb_msg);
            //
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

// deconnection de la file 
int m_deconnexion(MESSAGE *file){
    if(file == NULL) return -1;
    if(file->file->nb_proc_co>0){
        file->file->nb_proc_co -= 1;
        file->file = NULL;
        file->type = 0;
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
int m_envoi(MESSAGE *file, mon_message*msg, 
    size_t len, int msgflag){
    if(msgflag == 0){
    }
    else if(msgflag==O_NONBLOCK){
        if(file->file->last == file->file->capacite){
            errno = EAGAIN;
            return -1;
        }
        else {
            // verifier len pas trop grand
            if(file->file->longMax < len) return -1;
            int r = pthread_mutex_lock(&file->file->mutex);
            if(r == -1) return -1;
            //mon_message *m = malloc(sizeof(msg));
            //memcpy(m,msg,sizeof(mon_message));
            ///memmove(m,msg,sizeof(msg));
             printf(" M->TEXT %s\n", msg->mtext);
            printf(" %lu\n",msg->type);
            
             
            // printf("LES AUTRES %s\n",file->file->tabMessage[0].mtext);
            // printf("LES AUTRES %s\n",file->file->tabMessage[1].mtext);
            // printf("LES AUTRES %s\n",file->file->tabMessage[2].mtext);
            // affichage_mon_mess(m);
            // printf("position %d\n",file->file->last);
           // printf("\n");
           
            memcpy(file->file->tabMessage[file->file->last].mtext,msg->mtext,strlen(msg->mtext));
            //file->file->tabMessage[file->file->last].type=msg->type;
            //memcpy(file->file->tabMessage[file->file->last].type,msg->type,sizeof(msg->type));
            // printf("longmax %zu\n",file->file->longMax);
            // printf("sizeof +len %lu\n", sizeof(msg)+len);
            //memmove(file->file->tabMessage+(file->file->last),msg,sizeof(*msg));
            //memcpy(file->file->tabMessage+(file->file->last),m,sizeof(*m));
            //file->file->tabMessage[file->file->last] =  *m;
            // printf("coucou4\n");
            // printf("taiiiiiille %zu\n",m_nb(file));
            file->file->last++;

            printf("LES AUTRES 2 %s\n",file->file->tabMessage[0].mtext);
            printf("LES AUTRES 2 %s\n",file->file->tabMessage[1].mtext);
            printf("LES AUTRES 2 %s\n",file->file->tabMessage[2].mtext);
            
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

//supprimer element a la position i et decaler les autres
//ATTENTION UTILISER MUTEX
void suppression(MESSAGE *file, int pos){
    int r = pthread_mutex_lock(&file->file->mutex);
            if(r == -1) exit(0);
    
    for(int i=pos;i<m_nb(file)-1; i++){
        file->file->tabMessage[i]=file->file->tabMessage[i+1];
    }
    file->file->last--;
    msync(file->file, sizeof(file->file), MS_SYNC);
    r = pthread_mutex_unlock(&file->file->mutex);
}


// lire message 
ssize_t m_reception(MESSAGE *file, void *msg, size_t len, long type, int flags){
    if(flags==O_NONBLOCK){
        printf("bouh\n");
        //sizeof == taille en octet
        mon_message *msgvoulu=malloc(sizeof(mon_message)+len); 
        if(len< sizeof(msgvoulu)){
            errno = EMSGSIZE;
            return -1;
        }else if(m_nb(file)==0){
            errno = EAGAIN;
            return -1;
        }else{
            if(type== 0){
                printf("zero ok\n");
                //lire le premier message dans la file
                //memmove(msgvoulu,file->file->tabMessage[file->file->first],sizeof(mon_message)+len);
                msgvoulu=&file->file->tabMessage[file->file->first];
                printf("le message est %s\n",file->file->tabMessage[file->file->first].mtext);
                printf("le type ets %lu\n",file->file->tabMessage[file->file->first].type);
                //msgvoulu.mtext est vide x(
                printf("le message est %s\n",msgvoulu->mtext);
                printf("le type est %lu\n",msgvoulu->type);
                memmove(msg,&msgvoulu,sizeof(msgvoulu));
                             

                //memcpy(msg,&msgvoulu,sizeof(msgvoulu));
                
                suppression(file,file->file->first);
                return(sizeof(msgvoulu));
            }else if(type>0){
                //lire le premier message dont le type est type
                for(int i=0; i<m_nb(file);i++){
                    mon_message msgtmp=file->file->tabMessage[i];
                    if(type== msgtmp.type){
                        //on est ok
                        msgvoulu=&msgtmp;
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
                        msgvoulu=&msgtmp;
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
        printf("first : %d, last : %d \n", e->first, e->last);
        printf("nb_proc : %d\n",e->nb_proc_co);
        for(int i= e->first; i<e->last; i++){
            affichage_mon_mess(&(e->tabMessage[i]));
            printf("\n");
        }
    }
}
void affichage_mon_mess(mon_message *mm){
    // printf("mon_mess.type : %ld \n",mm->type);
    printf("type %ld mess %s\n",mm->type,mm->mtext);
    //printf(" sizeof %d\n",sizeof(mm->mtext));
    //printf("strlen %lu\n",strlen(mm->mtext));
    // char buf[256];
    // // char *type = "type : ";
    // // char *mess = ", mon mess : ";
    // char text[12];
    // memmove(text,mm->mtext,strlen(mm->mtext));
    // text[10] = '\0';
    // sprintf(buf,"type : %ld, mon mess : %s",mm->type,text);
    // write(STDOUT_FILENO,buf,sizeof(mm)+22);
    // printf("type : %ld, mon mess : %s", mm->type, mm->mtext);
  /// printf("mon mess: %d\n",sizeof(mm->mtext));

   
}

int enlever(const char *nom){
    int d=shm_open(nom,O_RDWR, 0666);
    if(d == -1) return -2;
    ftruncate(d,sizeof(enteteFile));
    enteteFile *mem=mmap(0,sizeof(enteteFile),PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
    if(munmap(mem, sizeof(enteteFile))==-1){
        return -1;
    }
    return 0;
}

int main(int argc, char const *argv[]){
    char * path = "/bloupsssss";
    shm_unlink(path);

    MESSAGE* m = m_connexion(path, O_RDWR|O_CREAT|O_EXCL, 3, 3, 256, S_IRUSR | S_IWUSR);
    
    affichage_message(m);
    printf("\n");
    MESSAGE *m1 = m_connexion(path,O_RDWR, 0);
    affichage_message(m1);
    printf("\n\n");

     //int t[2] = {-12, 99};
    char *t = "salou";
    printf("sizeof salut %lu\n",sizeof(t));
    mon_message *mes = malloc(sizeof(mon_message) + sizeof(t)+1);
    if( mes == NULL ) return -1;

    mes->type = (long) getpid(); /* comme type de message, on choisit l’identité
    //                             * de l’expéditeur */
    
    memmove( mes->mtext, t, sizeof(t)+1) ; /* copier les deux int à envoyer */
    
    //affichage_mon_mess(mes);
    printf("\n");
    int i = m_envoi(m,mes,sizeof(t)+1,O_NONBLOCK);
    if(i == 0){
        printf("Ok \n");
        affichage_message(m);
        printf("\n");
       //affichage_message(m1);
        //printf("\n");
    }
    char *t2 = "bonj";
      printf("sizeof bonj %lu\n",sizeof(t2));
   
    mon_message *mes22 = malloc(sizeof(mon_message) + sizeof(t2)+1);
    
    if( mes22 == NULL ) return -1;
   
    mes22->type = (long) getpid(); /* comme type de message, on choisit l’identité
    //                             * de l’expéditeur */
    // printf("test4\n");
    memmove( mes22->mtext, t2, sizeof(t2)+1) ; /* copier les deux int à envoyer */
    // printf("test5\n");
    //affichage_mon_mess(mes);
    printf("\n");
    int i2 = m_envoi(m,mes22,sizeof(t2)+1,O_NONBLOCK);
    if(i2 == 0){
        printf("Ok\n");
        affichage_message(m);
        printf("\n");
        //affichage_message(m1);
        //printf("\n");
    }
    // char *t3 = "aurevoir";
    // // //char * ti = "coucou";
    // mon_message *mes3 = malloc(sizeof(mon_message) + sizeof(t3)+1);
    // // printf("test2\n");
    // if( mes3 == NULL ) return -1;
    // // printf("test3\n");
    // mes3->type = (long) getpid(); /* comme type de message, on choisit l’identité
    // //                             * de l’expéditeur */
    // // printf("test4\n");
    // memmove( mes3->mtext, t3, sizeof(t3)+1) ; /* copier les deux int à envoyer */
    // // printf("test5\n");
    // //affichage_mon_mess(mes);
    // printf("\n");
    // int i3 = m_envoi(m,mes3,sizeof(t3)+1,O_NONBLOCK);
    // if(i3 == 0){
    //     printf("Ok \n");
    //     affichage_message(m);
    //     printf("\n");
    //     affichage_message(m1);
    //     printf("\n");
    // }

    printf("*********************************************************************************\n");
    // mon_message *mes2=malloc(sizeof(mon_message) + sizeof(t)+1);
    // int p= m_reception(m,mes2,300,0,O_NONBLOCK);
    // if(p!= -1){
    //         printf("Ok recu %d\n",p);
    //         affichage_mon_mess(mes2);
    // }
    // mon_message *mess=malloc(sizeof(mon_message) + sizeof(t)+1);
    // int p1= m_reception(m,mess,300,0,O_NONBLOCK);
    // if(p1!= -1){
    //         printf("Ok recu %d\n",p1);
    //         affichage_mon_mess(mess);
    // }

    // printf("\n test3333\n");
    // printf("taille %zu\n",m_nb(m));
   
    // sleep(15);
    printf("\ndeco %d\n",m_deconnexion(m));
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("destruc %d\n",m_destruction(path));
    printf("deco %d\n",m_deconnexion(m1));
    printf("destruc %d\n",m_destruction(path));
    // MESSAGE *m2 = m_connexion(path,O_RDWR, 0);
    // affichage_message(m2);
    affichage_message(m);
    affichage_message(m1);


    return 0;
}
