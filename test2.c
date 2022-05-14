#include "m_file.h"

void purger(void){
   char c;
   while((c=getchar()) != '\n' && c != EOF){
   }
}

int tailleReelle(char *t){
    int taille = 0;
    while(taille<strlen(t) && t[taille]!='\0')
        taille++;
    return taille;
}

int main(int argc, char const *argv[]){
    // /dev/shm

    char path[10] = {0};
    printf("Nom de la file commence par / et de moins de 9 caractere ou file anonyme (n) :    ");
    scanf("%s", path);
    // printf("path : %s %ld\n",path,sizeof(path));
    purger();
    while(path[0] != '/' && strcmp(path, "n") != 0 && sizeof(path) > 10){
        printf("Erreur : Nom de la file commence par / et de moins de 9 caractere ou file anonyme (n) :    ");
        scanf("%s", path);
        purger();
    }
    shm_unlink(path);

    MESSAGE *m = malloc(sizeof(MESSAGE));

    char choix;
    if(strcmp(path, "n") == 0) choix = 'c';
    else {
        printf("\nCreation (c) ou connection (n) a une file :    ");
        scanf("%c", &choix);
        purger();
        while(choix != 'c' && choix != 'n'){
            printf("Erreur : Creation (c) ou connection (n) a une file :    ");
            scanf("%c", &choix);
            purger();
        }
    }
    
    if(choix == 'n'){
        int option;
        printf("\nChoix option : \nlecture et ecriture (0), \nlecture (1), \necriture (2) : \n     ");
        scanf("%d", &option);
        purger();
        while(option < 0 || option > 2){
            printf("Erreur : Choix option : \nlecture et ecriture (0), \nlecture (1), \necriture (2) : \n     ");
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
        printf("Nombre de message :      ");
        scanf("%d",&nbMsg);
        purger();

        // taille max des messages
        int tailleMsg;
        printf("Taille max des messages :      ");
        scanf("%d",&tailleMsg);
        purger();

        // option de la file
        int optionC;
        printf("\n----- Choix option : -----");
        printf("\nlecture et ecriture en creation (0), \nlecture et ecriture en creation et execution (1)");
        printf("\nlecture en creation (2), \nlecture en creation et execution (3)");
        printf("\necriture en creation (4), \necriture en creation et execution (5) : \n     ");
        scanf("%d", &optionC);
        purger();
        while(optionC < 0 || optionC > 5){
            printf("Erreur : Choix option : ");
            printf("\nlecture et ecriture en creation (0), \nlecture et ecriture en creation et execution (1)");
            printf("\nlecture  (2), \nlecture en creation et execution (3)");
            printf("\necriture en creation (4), \necriture en creation et execution (5) : \n     ");
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
        printf("Choix mode : \nlecture et ecriture (0), \nlecture (1), \necriture (2) : \n     ");
        scanf("%d", &mode);
        purger();
        while(mode < 0 || mode > 2){
            printf("Erreur : Choix mode : \nlecture et ecriture (0), \nlecture (1), \necriture (2) : \n     ");
            scanf("%d", &mode);
            purger();
        }
        if(mode == 0) m = m_connexion(path, option, 3, nbMsg, tailleMsg, S_IRUSR | S_IWUSR);
        else if(mode == 1) m = m_connexion(path, option, 3, nbMsg, tailleMsg, S_IRUSR);
        else m = m_connexion(path, option, 3, nbMsg, tailleMsg, S_IWUSR);

    }
    if(m == NULL) {
        printf("Erreur lors de la connection à la file\n");
        return 1;
    }
    affichage_message(m);
    printf("pid proc %d \n",getpid());

    int arret = 1;
    while (arret == 1){
        printf("\n");
        printf("----- Veuillez choisir/ecrire votre action : -----\n");
        printf("Envoyer un message (e), \nRecevoir un message (r), \n");
        printf("S'enregistrer (s),\n");
        printf("Se deconnecter (d), \nDetruire la file (n)\n     ");
        char choix; 
        scanf("%c", &choix);
        purger();
        if(choix == 'e'){
            char *t = NULL;;
            size_t size = m_message_len(m)*m_message_len(m);
            printf("Veuillez ecrire votre message : \n     ");
            getline(&t, &size, stdin);
            int taille = tailleReelle(t);
            printf("taille %d\n", taille);
            char tMess[taille];
            memcpy(tMess,t,taille-1);
            tMess[taille-1] = '\0';
            free(t);
            mon_message *mess = malloc(sizeof(mon_message) + sizeof(tMess));
            mess->type = (long) getpid();
            mess->len = sizeof(tMess);
            memmove(mess->mtext, tMess, sizeof(tMess));
            int mode;
            printf("Et votre mode (bloquant (0) ou non bloquant (1)) :      ");
            scanf("%d",&mode);
            purger();
            while(mode != 0 && mode != 1){
                printf("Erreur \nChoix du mode (bloquant (0) ou non bloquant (1)) :      ");
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
            printf("\nChoix type message a recevoir (0, pid, -pid):      ");
            scanf("%ld", &type);
            purger();

            // mode, bloquant ou non
            int mode;
            printf("Et votre mode (bloquant (0) ou non bloquant (1)) :      ");
            scanf("%d",&mode);
            purger();
            while(mode != 0 && mode != 1){
                printf("Erreur \nChoix du mode (bloquant (0) ou non bloquant (1)) :      ");
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
            printf("\nChoix signal : SIGUSR1 (1), SIGUSR2 (2) :      ");
            scanf("%d", &sig);
            purger();
            while(sig < 1 || sig > 2){
                printf("Erreur : Choix signal : SIGUSR1 (1), SIGUSR2 (2) :      ");
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
            printf("\nChoix type signal en attente (pid):      ");
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


    return 0;
}
