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
    return (file->file->last)/(sizeof(mon_message)+file->file->longMax);
}
size_t m_size_messages(MESSAGE *file){
    return (sizeof(mon_message)+file->file->longMax);
}


// connexion a une file de message ou creation
MESSAGE *m_connexion(const char *nom, int options, int nb, ...){ //size_t nb_msg, size_t len_max, mode_t mode
        // nb = 0 ou 3 -> nb darguments en plus

    printf("BAH\n");
    MESSAGE *mess=malloc(sizeof(MESSAGE));
    enteteFile *entete;
    //option ne contient pas ocreat
    if((options == O_RDONLY)||(options == O_WRONLY)||(options == O_RDWR)){
        printf("MERDE\n");
        //mess=malloc(sizeof(MESSAGE));
        // on ne cree pas la file on a que 2 arg 
        int d = shm_open(nom, options,S_IRUSR | S_IWUSR);
        if(d<0) return NULL;
        struct stat buf;
        int r =fstat(d,&buf);
        if(r== -1) return NULL;
        entete=mmap(NULL,buf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,d,0);
        mess->type=buf.st_mode;
        entete->nb_co+=1;
        mess->file=entete;

        
    }else{
        printf("hey\n");
        va_list param;
        va_start(param, nb);
        size_t nb_msg;
        size_t len_max;
        mode_t mode;
        nb_msg = (size_t) va_arg(param, size_t);
        len_max = (size_t) va_arg(param, size_t);
        mode = (mode_t)va_arg(param, int);
        
        size_t tailleent=sizeof(enteteFile)+(sizeof(mon_message)+len_max) *nb_msg;

        if(nom == NULL){
            //file ano
            entete=mmap(NULL, tailleent, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,-1,0);
        }else{
            int d=shm_open(nom, options, mode);
            ftruncate(d,sizeof(enteteFile));
            if(d<0) return NULL;
            entete=mmap(NULL,tailleent,PROT_READ|PROT_WRITE, MAP_SHARED,d,0);
        }
        int r=initialiser_mutex(&entete->mutex);
        if(r ==-1) return NULL;
        r=pthread_mutex_lock(&entete->mutex);
        if(r == -1) return NULL;
        
        entete->capacite=nb_msg;
        entete->first=0;
        entete->last=0;
        entete->longMax=len_max;
        entete->nb_co=1;
        
        mess->type=mode;
        mess->file=entete;
       
        msync(&mess->file, sizeof(mess->file), MS_SYNC);
        r=pthread_mutex_unlock(&entete->mutex);
        va_end(param);
    }
    return mess;
    
    
}

// deconnection de la file 
int m_deconnexion(MESSAGE *file){
    file->file->nb_co-=1;
    if(munmap(file->file, 
        sizeof(enteteFile)+(sizeof(mon_message)+file->file->longMax) *file->file->capacite)==-1){
            file->file->nb_co+=1;
            return -1; 
        } 
    return 0;
}


//destruction de la file
int m_destruction(const char *nom){
    if(shm_unlink(nom)==-1){
        return -1;
    }
    return 0;

}


// envoie de message
int m_envoi(MESSAGE *file, const void *msg, 
    size_t len, int msgflag){
    if(msgflag == 0){
        // faire avec signaux


    }
    else if(msgflag==O_NONBLOCK){
        if(m_nb(file) == file->file->capacite){
            errno = EAGAIN;
            return -1;
        }
        else {
            printf("len : %zu\n",len);

            // verifier len pas trop grand
            if(file->file->longMax < len) return -1;

            int r = pthread_mutex_lock(&file->file->mutex);
            if(r == -1) return -1;
            memcpy(file->file->messages+(file->file->last),msg,sizeof(mon_message)+len);
            // memmove(file->file->messages+(file->file->last),msg,sizeof(mon_message)+len);

            file->file->last+=sizeof(mon_message)+file->file->longMax;
            msync(file->file, sizeof(file->file), MS_SYNC);
            r = pthread_mutex_unlock(&file->file->mutex);
            if(r == -1) return -1;
        }
    }
    else return -1;
    // printf("coucoufin\n");
    return 0;
}

// lire message
ssize_t m_reception(MESSAGE *file, void *msg, 
    size_t len, long type, int flags){

    return 0;
}


void affichage_message(MESSAGE *m){
    if(m==NULL) printf("null la\n");
    else if(m->file->nb_co==0){ //mystere
        printf("null ici\n");
    }else{
        printf("type : %ld\n",m->type);
        affichage_entete(m->file,m_nb(m),m_size_messages(m));
    } 
}
void affichage_entete(enteteFile *e, size_t nb, size_t l){
    if(e->nb_co==0){
        printf("null\n");
    }else{
        printf("long max : %zu\n",e->longMax);
        printf("capacite : %zu\n", e->capacite);
        printf("first : %d, last : %d \n", e->first, e->last);
        printf("nb_co : %d\n",e->nb_co);
        int i=0;
        printf("nbMess : %zu",nb);
        for(int i=0;i<nb;i++){
            int j = i * l;
            char type[sizeof(long)];
            char len[sizeof(long)];
            while(j < sizeof(mon_message) + i*l){

                j++;
            }

        }
        while(i<e->last){
            // if(e->messages[i]=='\0') printf("\0");
            if(i%(sizeof(mon_message)+e->longMax) == 0) printf("\n");
            printf("%c",e->messages[i]);
            
            i++;
        }
        printf("\n");
        // for(int i=0;i<nb;i++){
        //     printf("typeMess : ");
        //     for(int j=0;i<sizeof(mon_message);j++){
        //         // if(e->messages[j+(i*(sizeof(mon_message)+e->longMax))]!='\0') 
        //             printf("%c",e->messages[j+(i*(sizeof(mon_message)+e->longMax))]);
        //     }
        //     printf("\nmessage : ");
        //     for(int j=0;j<e->longMax;j++){
        //         // if(e->messages[j+sizeof(mon_message)+(i*(sizeof(mon_message)+e->longMax))]!='\0') 
        //             printf("%c",e->messages[j+sizeof(mon_message)+(i*(sizeof(mon_message)+e->longMax))]);
        //     }
        //     printf("\n");
        // }
    }
}

// void affichage_mon_mess(mon_message *mm){
//     // // printf("mon_mess.type : %ld \n",mm->type);
//     // char buf[256];
//     // // char *type = "type : ";
//     // // char *mess = ", mon mess : ";
//     // char text[12];
//     // memmove(text,mm->mtext,strlen(mm->mtext));
//     // text[10] = '\0';
//     // sprintf(buf,"type : %ld, mon mess : %s",mm->type,text);
//     // write(STDOUT_FILENO,buf,sizeof(mm)+22);
//     printf("type : %ld, mon mess : %s", mm->type, mm->mtext);
// }

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
    // /dev/shm

    // char n = 'a'; 
    // for(int i=0;i<26;i++){
    //     // printf("n:%c ",n);
    //     char nom[2];
    //     nom[0] = '/';
    //     nom[1] = n;
    //     // printf("nom:%s\n",nom);
    //     printf("enlever %s:%d\n",nom,enlever(nom)); 
    //     n++;
    // }
    // printf("\n\n");

    char * path = "/a";
    shm_unlink(path);

    MESSAGE* m = m_connexion(path, O_RDWR|O_CREAT|O_EXCL, 3, 3, 10, S_IRUSR | S_IWUSR);
    
    // MESSAGE *m = m_connexion(path,O_RDWR, 0);
    affichage_message(m);
    printf("\n");
    MESSAGE *m1 = m_connexion(path,O_RDWR, 0);
    affichage_message(m1);
    printf("\n\n");

    // int t[2] = {-12, 99};
    char t[] = "salut";
    printf("sizeof : %zu\n",sizeof(t));
    // //char * ti = "coucou";
    mon_message *mes = malloc(sizeof(mon_message) + strlen(t));
    // printf("test2\n");
    if( mes == NULL ) return -1;
    // printf("test3\n");
    mes->type = (long) getpid(); /* comme type de message, on choisit l’identité
    //                             * de l’expéditeur */
    mes->len = strlen(t);
    // printf("test4\n");
    memmove( mes->mtext, t, strlen(t)) ; /* copier les deux int à envoyer */
    // printf("test5\n");
    // affichage_mon_mess(mes);
    // printf("\n");
    int i = m_envoi(m,mes,strlen(t),O_NONBLOCK);
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("\n\n");

    char t2[] = "bonjour";
    mon_message *mes1 = malloc(sizeof(mon_message) + sizeof(t2));
    mes1->type = (long) getpid();
    memmove( mes1->mtext, t2, sizeof(t2)) ;
    i = m_envoi(m,mes1,sizeof(t2),O_NONBLOCK);
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("\n\n");

    char t3[] = "coucou";
    mon_message *mes2 = malloc(sizeof(mon_message) + sizeof(t3));
    mes2->type = (long) getpid();
    memmove( mes2->mtext, t3, sizeof(t3)) ;
    i = m_envoi(m,mes2,sizeof(t3),O_NONBLOCK);
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("\n\n");

    char t4[] = "aurevoir";
    mon_message *mes3 = malloc(sizeof(mon_message) + sizeof(t4));
    mes3->type = (long) getpid();
    memmove( mes3->mtext, t4, sizeof(t4)) ;
    // for(int j=0;j<4;j++){
        i = m_envoi(m,mes3,sizeof(t4),O_NONBLOCK);
        if(i == 0){
            printf("Ok %d\n",i);
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

    // }
    
    

    // sleep(15);
    printf("\ndeco %d\n",m_deconnexion(m));
    // affichage_message(m);
    // affichage_message(m1);
    // printf("destruc %d\n",m_destruction(path));
    printf("deco %d\n",m_deconnexion(m1));
    printf("destruc %d\n",m_destruction(path));
    // MESSAGE *m2 = m_connexion(path,O_RDWR, 0);
    // affichage_message(m2);
    // affichage_message(m);
    // affichage_message(m1);


    return 0;
}
