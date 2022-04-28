#include "m_file.h"
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
    mes1->len = strlen(t2);
    memmove( mes1->mtext, t2, sizeof(t2)) ;
    i = m_envoi(m,mes1,sizeof(t2),O_NONBLOCK);
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("\n\n");

    char t3[] = "coucou";
    mon_message *mes2 = malloc(sizeof(mon_message) + sizeof(t3));
    mes2->type = (long) getpid();
    mes2->len = strlen(t3);
    memmove( mes2->mtext, t3, sizeof(t3)) ;
    i = m_envoi(m,mes2,sizeof(t3),O_NONBLOCK);
    affichage_message(m);
    printf("\n");
    affichage_message(m1);
    printf("\n\n");

    char t4[] = "aurevoir";
    mon_message *mes3 = malloc(sizeof(mon_message) + sizeof(t4));
    mes3->type = (long) getpid();
    mes3->len = strlen(t4);
    memmove( mes3->mtext, t4, sizeof(t4)) ;
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

    printf("**********************************************************************\n");
    int len_mess=300;
    mon_message *mess=malloc(sizeof(mon_message) +len_mess);
    int p1= m_reception(m,mess,len_mess,0,O_NONBLOCK);
    if(p1!= -1){
            printf("Ok recu %d\n",p1);
            //affichage_mon_mess(mess);
    }
    printf("*****************\n");
    affichage_message(m);
    printf("*****************\n");
    mon_message *mess1=malloc(sizeof(mon_message) + len_mess);
    int p2= m_reception(m,mess1,len_mess,-getpid(),O_NONBLOCK);
    if(p2!= -1){
            printf("Ok recu %d\n",p2);
            //affichage_mon_mess(mess);
    }
    printf("*****************\n");
    affichage_message(m);
    printf("*****************\n");
    mon_message *mess2=malloc(sizeof(mon_message) + len_mess);
    int p3= m_reception(m,mess2,len_mess,getpid(),O_NONBLOCK);
    if(p3!= -1){
            printf("Ok recu %d\n",p3);
            //affichage_mon_mess(mess);
    }
    printf("*****************\n");
    affichage_message(m);
    printf("*****************\n");

    char t5[] = "aurevoir";
    mon_message *mes5 = malloc(sizeof(mon_message) + sizeof(t5));
    mes5->type = (long) getpid();
    mes5->len = strlen(t5);
    memmove( mes5->mtext, t5, sizeof(t5)) ;
    i = m_envoi(m,mes5,sizeof(t5),O_NONBLOCK);
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
