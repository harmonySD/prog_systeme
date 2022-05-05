#include "m_file.h"

void purger(void){
   char c;
   while((c=getchar()) != '\n' && c != EOF){
   }
}

int main(int argc, char const *argv[]){
    // /dev/shm

    char path[10] = {0};
    printf("Nom de la file commence par / et de moins de 9 caractere : ");
    scanf("%s", path);
    // printf("path : %s %ld\n",path,sizeof(path));
    purger();
    while(path[0] != '/' || sizeof(path) > 10){
        printf("Erreur : Nom de la file commence par / et de moins de 9 caractere : ");
        scanf("%s", path);
        purger();
    }
    // shm_unlink(path);

    MESSAGE *m = malloc(sizeof(MESSAGE));

    char choix;
    printf("\nCreation (c) ou connection (n) a une file : ");
    scanf("%c", &choix);
    purger();
    while(choix != 'c' && choix != 'n'){
        printf("Erreur : Creation (c) ou connection (n) a une file : ");
        scanf("%c", &choix);
        purger();
    }
    if(choix == 'n'){
        int option;
        printf("\nChoix option : lecture et ecriture (0), lecture (1), ecriture (2) : ");
        scanf("%d", &option);
        purger();
        while(option < 0 || option > 2){
            printf("Erreur : Choix option : lecture et ecriture (0), lecture (1), ecriture (2) : ");
            scanf("%d", &option);
            purger();
        }
        if(option == 0) m = m_connexion(path, O_RDWR, 0);
        else if(option == 1) m = m_connexion(path, O_RDONLY, 0);
        else m = m_connexion(path, O_WRONLY, 0);
    }
    else{
        // nombre de message
        int nbMsg;
        printf("Nombre de message : ");
        scanf("%d",&nbMsg);
        purger();

        // taille max des messages
        int tailleMsg;
        printf("Taille max des messages : ");
        scanf("%d",&tailleMsg);
        purger();

        // option de la file
        int optionC;
        printf("\n----- Choix option : -----");
        printf("\nlecture et ecriture en creation (0), \nlecture et ecriture en creation et execution (1)");
        printf("\nlecture  (2), \nlecture en creation et execution (3)");
        printf("\necriture en creation (4), \necriture en creation et execution (5) : ");
        scanf("%d", &optionC);
        purger();
        while(optionC < 0 || optionC > 5){
            printf("Erreur : Choix option : ");
            printf("\nlecture et ecriture en creation (0), \nlecture et ecriture en creation et execution (1)");
            printf("\nlecture  (2), \nlecture en creation et execution (3)");
            printf("\necriture en creation (4), \necriture en creation et execution (5) : ");
            scanf("%d", &optionC);
            purger();
        }
        int option;
        if(optionC == 0) option = O_RDWR|O_CREAT;
        else if(optionC == 1) option = O_RDWR|O_CREAT|O_EXCL;
        else if(optionC == 2) option = O_RDONLY|O_CREAT;
        else if(optionC == 3) option = O_RDONLY|O_CREAT|O_EXCL;
        else if(optionC == 4) option = O_WRONLY|O_CREAT;
        else option = O_WRONLY|O_CREAT|O_EXCL;

        // mode de la file
        int mode;
        printf("Choix mode : lecture et ecriture (0), lecture (1), ecriture (2) : ");
        scanf("%d", &mode);
        purger();
        while(mode < 0 || mode > 2){
            printf("Erreur : Choix mode : lecture et ecriture (0), lecture (1), ecriture (2) : ");
            scanf("%d", &mode);
            purger();
        }
        if(mode == 0) m = m_connexion(path, option, 3, nbMsg, tailleMsg, S_IRUSR | S_IWUSR);
        else if(mode == 1) m = m_connexion(path, option, 3, nbMsg, tailleMsg, S_IRUSR);
        else m = m_connexion(path, option, 3, nbMsg, tailleMsg, S_IWUSR);

    }
    affichage_message(m);

    // MESSAGE* m = m_connexion(path, O_RDWR|O_CREAT|O_EXCL, 3, 3, 10, S_IRUSR | S_IWUSR);
 
    affichage_message(m);
    printf("pid proc %d \n",getpid());

    if(m == NULL) return 1;

    int arret = 1;
    while (arret == 1){
        printf("\n");
        printf("----- Veuillez choisir/ecrire votre action : -----\n");
        printf("e pour envoyer un message, \nr pour recevoir un message, \n");
        printf("s pour senregistrer,\n");
        printf("d pour se deconnecter, \nn pour detruire la file\n");
        char choix; 
        scanf("%c", &choix);
        purger();
        if(choix == 'e'){
            // regarder pour taille, car ici taille fixe pas correcte
            char tMess[m_message_len(m)];
            printf("Veuillez ecrire votre message de taille inferieur à %ld (sinon il sera coupe a lenvoie): ", m_message_len(m));
            scanf("%s",tMess);
            purger();
            mon_message *mess = malloc(sizeof(mon_message) + sizeof(tMess));
            mess->type = (long) getpid();
            mess->len = sizeof(tMess);
            memmove(mess->mtext, tMess, sizeof(tMess));
            int mode;
            printf("Et votre mode (0 pour bloquant ou 1 pour non bloquant) : ");
            scanf("%d",&mode);
            purger();
            while(mode != 0 && mode != 1){
                printf("Erreur \nChoix du mode (0 pour bloquant ou O_NONBLOCK pour non bloquant : ");
                scanf("%d",&mode);
                purger();
            }
            int j;
            if(mode == 1) j = m_envoi(m, mess, sizeof(tMess), O_NONBLOCK);
            else j = m_envoi(m, mess, sizeof(tMess), 0);
            if(j == 0){
                printf("Ok envoie %d\n", j);
                affichage_message(m);
            }
            else if(j == -1 && errno == EAGAIN)
                printf("File pleine, attendez un peu\n");
            else 
                printf("erreur\n");
            printf("pid proc %d \n",getpid());

        }
        else if(choix == 'r'){
            int len_mess = m_message_len(m);
            mon_message *mess = malloc(sizeof(mon_message) + len_mess);

            // typoe du message a recevoir
            long type;
            printf("\nChoix type message a recevoir : ");
            scanf("%ld", &type);
            purger();

            // mode, bloquant ou non
            int mode;
            printf("Et votre mode (0 pour bloquant ou 1 pour non bloquant) : ");
            scanf("%d",&mode);
            purger();
            while(mode != 0 && mode != 1){
                printf("Erreur \nChoix du mode (0 pour bloquant ou O_NONBLOCK pour non bloquant : ");
                scanf("%d",&mode);
                purger();
            }
            
            int j;
            if(mode == 1) j = m_reception(m, mess, len_mess, type, O_NONBLOCK, 0);
            else j = m_reception(m, mess, len_mess, type, 0, 0);
            if(j != -1){
                printf("Ok recu %d\n", j);
                affichage_mon_message(mess);            
            }
            else if(j == -1 && errno == EMSGSIZE)
                printf("taille donner trop petite\n");
            else if(j == -1 && errno == EAGAIN)
                printf("File vide, attendez un peu\n");
            else 
                printf("erreur\n");
            affichage_message(m);
            printf("pid proc %d \n",getpid());
        }
        else if(choix == 's'){
            int sig;
            printf("\nChoix signal : SIGUSR1 (1), SIGUSR2 (2) : ");
            scanf("%d", &sig);
            purger();
            while(sig < 1 || sig > 2){
                printf("Erreur : Choix signal : SIGUSR1 (1), SIGUSR2 (2) : ");
                scanf("%d", &sig);
                purger();
            }
            int signal;
            if(sig == 1) signal = SIGUSR1;
            else signal = SIGUSR2;

            //pour signaux
            struct sigaction  str = {0};
            str.sa_handler = handler;
            sigaction(signal,&str,NULL);

            long type;
            printf("\nChoix type signal en attente : ");
            scanf("%ld", &type);
            purger();

            int enre = enregistrement(m, signal, type);
            printf("enregistrement : %d\n",enre);
            affichage_message(m);
            printf("pid proc %d \n",getpid());

        }
        else if(choix == 'd'){
            printf("deco %d\n",m_deconnexion(m));
        }
        else if(choix == 'n'){
            printf("destruc %d\n",m_destruction(path));
            arret = 0;
        }



    }
    

    // printf("\n********************RECEVOIR***************************\n\n");

    

    

    // mon_message *mess2=malloc(sizeof(mon_message) + len_mess);
    // int p2 = m_reception(m, mess2, len_mess, -getpid(), O_NONBLOCK, 0);
    // if(p2 != -1){
    //     printf("Ok recu %d\n",p2);
    //     affichage_mon_message(mess2);
    // }
    // affichage_message(m);
    // printf("\n");

    // mon_message *mess3=malloc(sizeof(mon_message) + len_mess);
    // int p3 = m_reception(m, mess3, len_mess, getpid(), O_NONBLOCK,0);
    // if(p3 != -1){
    //     printf("Ok recu %d\n",p3);
    //     affichage_mon_message(mess3);
    // }
    // affichage_message(m);
    // printf("\n");


    // printf("\ndeco %d\n",m_deconnexion(m));
    // printf("deco %d\n",m_deconnexion(m1));
    // printf("destruc %d\n",m_destruction(path));



    return 0;
}
