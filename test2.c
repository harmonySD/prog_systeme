#include "m_file.h"

int main(int argc, char const *argv[]){
    // /dev/shm

    char * path = "/a";
   // shm_unlink(path);

    //MESSAGE* m = m_connexion(path, O_RDWR|O_CREAT|O_EXCL, 3, 3, 10, S_IRUSR | S_IWUSR);
     MESSAGE *m = m_connexion(path,O_RDWR, 0);
    affichage_message(m);
    printf("\n");
    MESSAGE *m1 = m_connexion(path,O_RDWR, 0);
    affichage_message(m1);
    printf("\n\n");

    int signal = SIGUSR1;
    //pour signaux
    struct sigaction  str = {0};
    str.sa_handler=handler;
    sigaction(signal,&str,NULL);
    
 
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("\n\n");

    if(m==NULL ) return 1;
    
    printf("*********************ENVOIE************************\n\n");
    

    // int t[2] = {-12, 99};
    for(int i=0; i < 5; i++){
        char t[] = "salut";
        t[sizeof(t)-1] = i +'0';
        mon_message *mes = malloc(sizeof(mon_message) + sizeof(t));
        mes->type = (long) getpid();
        mes->len = sizeof(t);
        memmove( mes->mtext, t, sizeof(t)) ;
        int j = m_envoi(m, mes, sizeof(t), O_NONBLOCK);
        if(j == 0){
            printf("Ok envoie %d\n",i);
            affichage_message(m);
            printf("\n");
        }
        else if(j == -1 && errno == EAGAIN){
            printf("file pleine, attendez un peu\n");
        }
        else {
            printf("erreur\n");
        }
    }

    printf("\n********************RECEVOIR***************************\n\n");
    // enre = enregistrement(m, signal, getpid());
    // printf("enregistrement : %d\n\n",enre);

    int enre = enregistrement(m, signal, 9742);
    printf("enregistrement : %d\n",enre);

    int len_mess = m_message_len(m);
    mon_message *mess=malloc(sizeof(mon_message) + len_mess);
    int p1= m_reception(m,mess,len_mess,0,O_NONBLOCK,0);
    if(p1!= -1){
        printf("Ok recu %d\n",p1);
        affichage_mon_message(mess);
    }
    affichage_message(m);
    printf("nop\n");

    mon_message *mess1=malloc(sizeof(mon_message) + len_mess);
    int p2= m_reception(m,mess1,len_mess,-getpid(),O_NONBLOCK,0);
    if(p2!= -1){
        printf("Ok recu %d\n",p2);
        affichage_mon_message(mess1);
    }
    affichage_message(m);
    printf("\n");

    mon_message *mess2=malloc(sizeof(mon_message) + len_mess);
    int p3= m_reception(m, mess2, len_mess, getpid(), O_NONBLOCK,0);
    if(p3!= -1){
        printf("Ok recu %d\n",p3);
        affichage_mon_message(mess2);
    }
    affichage_message(m);
    printf("\n");


 
    printf("FINITO\n");

    printf("\ndeco %d\n",m_deconnexion(m));
    printf("deco %d\n",m_deconnexion(m1));
    printf("destruc %d\n",m_destruction(path));



    return 0;
}
