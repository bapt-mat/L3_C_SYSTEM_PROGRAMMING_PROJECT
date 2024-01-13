#include "types.h"
Tableau_Fils fils;
int nb_fils;
int code_arret;
int pos_fils=0;

void usage (char * prog){
    couleur(ROUGE); 
    fprintf(stdout,"Aie caramba! %s '2 < nb archivistes' '3 < nb thèmes < 30' \n", prog);
    couleur(REINIT);
}

/*----------------------------------------*
| Gestionnaire de signaux
*-----------------------------------------*/
void mon_sigaction(int signal, void (*f)(int)){
    struct sigaction action;
    action.sa_handler = f;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(signal,&action,NULL);
}

/*----------------------------------------*
| Arret du programme initial
*-----------------------------------------*/
void arret(){
    code_arret = 1;
}

/*----------------------------------------*
| Terminaison du programme et des fils
*-----------------------------------------*/
void destruction_fils(){
    for (int i=0; i<=nb_fils; i++){
        kill(fils[i], SIGINT);
    }
}

/*----------------------------------------*
|                                         |
| Selectionne l'action du journaliste
|           70% consultation
|           20% production
|           10% effacement 
|                                         |
*-----------------------------------------*/
char action_journaliste(){
    int choix;
    choix =rand()%+11;
    if (choix<=7){
        return 'c';
    }

    if (choix==8 || choix==9){
        return 'p';
    }

    return 'e';
}

/*----------------------------------------*
| Thème aléatoire
*-----------------------------------------*/
int theme_alea(int nb_themes){
    return rand()%+nb_themes;
}

/*----------------------------------------*
| Article aléatoire
*-----------------------------------------*/
int article_alea(){
    return rand()%+MAX_ARTICLE;
}


int main(int argc, char*argv[]){
    srand(time(NULL));
    /*----------------------------------------*
    |                                         |
    | ~~~  VARIABLES ET ARGUMENTS  ~~~
    |                                         |
    *-----------------------------------------*/

    if (argc<3){
        usage(argv[0]);
        exit(-1);
    }

    int nb_archivistes=atoi(argv[1]);
    if (nb_archivistes < 2){
        usage(argv[0]);
        exit(-1);
    }

    int nb_themes=atoi(argv[2]) ;
    if (nb_themes < 3 || nb_themes > 29){ //pb d'espace d'adressage quand > 30 themes
        usage(argv[0]);
        exit(-1);
    }

    /*----------------------------------------*
    | Variables
    *-----------------------------------------*/
    FILE * fichier_cle;
    key_t cle_theme[nb_themes];
    key_t cle_fm;
    key_t cle_sem;
    key_t cle_smp_file;
    key_t cle_smp_nb_lecteurs;
    key_t cle_smp_nb_ecrivains;

    /*----------------------------------------*
    |                                         |
    |     ~~~ GESTION DES SIGNAUX ~~~
    |                                         |
    *-----------------------------------------*/
    for (int i=1; i<=22; i++){
        if (i!=17){
            mon_sigaction(i, arret);
        }
    }   

    /*----------------------------------------*
    |                                         |
    |         ~~~ FICHIER CLÉ ~~~
    |                                         |
    *-----------------------------------------*/
    fichier_cle = fopen(FICHIER_CLE,"r");
    if (fichier_cle==NULL){
        /* Création */
        if (errno==ENOENT){
                fichier_cle=fopen(FICHIER_CLE,"w");
                if (fichier_cle==NULL){
                    fprintf(stderr,"(Initial) Création clé impossible\n");
                    exit(-1);
                }
        }
        /* Autre probleme */
        else{
            fprintf(stderr,"(Initial) Création clé impossible\n");
            exit(-1);
        }
    }

    /*----------------------------------------*
    |                                         |
    |             ~~~ CLÉS ~~~
    |                                         |
    *-----------------------------------------*/

    /*----------------------------------------*
    | Clés pour les SMP du contenu des thèmes
    *-----------------------------------------*/
    for (int i=0; i<nb_themes; i++){
        cle_theme[i] = ftok(FICHIER_CLE, i);
        if (cle_theme[i] == -1){
            fprintf(stderr, "(Initial) Problème création clé SMP\n");
            exit(-1);
        }
    }

    /*----------------------------------------*
    | Clé pour le SMP de la file d'attente
    *-----------------------------------------*/
    cle_smp_file = ftok(FICHIER_CLE, code_cle_smp_file);
    if (cle_smp_file == -1){
        fprintf(stderr, "(Initial) Problème création clé SMP file d'attente\n");
        exit(-1);
    }

    /*----------------------------------------*
    | Clé pour le SMP des nombres de lecteurs
    *-----------------------------------------*/
    cle_smp_nb_lecteurs = ftok(FICHIER_CLE, code_cle_smp_nb_lecteurs);
    if (cle_smp_nb_lecteurs == -1){
        fprintf(stderr, "(Initial) Problème création clé SMP nombre lecteurs\n");
        exit(-1);
    }

    /*----------------------------------------*
    | Clé pour le SMP des nombres d'écrivains
    *-----------------------------------------*/
    cle_smp_nb_ecrivains = ftok(FICHIER_CLE, code_cle_smp_nb_ecrivains);
    if (cle_smp_nb_ecrivains == -1){
        fprintf(stderr, "(Initial) Problème création clé SMP nombre écrivains\n");
        exit(-1);
    }

    /*----------------------------------------*
    | Clé pour la file de message
    *-----------------------------------------*/
    cle_fm = ftok(FICHIER_CLE, code_cle_fm);
    if (cle_fm == -1){
        fprintf(stderr, "(Initial) Problème création clé file de message\n");
        exit(-1);
    }

    /*----------------------------------------*
    | Clés pour l'ensemble de sémaphores 
    *-----------------------------------------*/
    cle_sem = ftok(FICHIER_CLE, code_cle_sem);
    if (cle_sem == -1){
        fprintf(stderr, "(Initial) Problème création clé sémaphore\n");
        exit(-1);
    }

    /*----------------------------------------*
    |                                         |
    |              ~~~ IPC ~~~
    |                                         |
    *-----------------------------------------*/

    /*----------------------------------------*
    |                                         |
    | ~~~ SEGMENTS DE MÉMOIRES PARTAGÉS  ~~~
    |                                         |
    *-----------------------------------------*/

    /*----------------------------------------*
    | Création des SMP (Thèmes)
    *-----------------------------------------*/
    int id_themes[nb_themes]; // Tableau des id des SMP (thèmes)
    for (int i=0; i<nb_themes; i++){
        id_themes[i] = shmget(cle_theme[i], MAX_ARTICLE*TAILLE_ARTICLE*sizeof(char), IPC_CREAT|IPC_EXCL|0660);
        //fprintf(stderr, "Creation theme (SMP) %d\n", i);
        if (id_themes[i] == -1){
            fprintf(stderr, "(Initial) Problème création SMP n°%d\n",i);
        }
    }

    /*----------------------------------------*
    | Attachement des SMP (Thèmes)
    *-----------------------------------------*/
    char * adr_themes[nb_themes]; // Tableau d'adresse des SMP (thèmes)
    for (int i=0; i<nb_themes; i++){
        adr_themes[i] = shmat(id_themes[i], NULL, 0);
        if (adr_themes[i] == (void*)-1){
            fprintf(stderr, "(Initial) Problème attachement a l'EA du SMP n°%d\n",i);
        }

        memset(adr_themes[i], 0, sizeof(MAX_ARTICLE*TAILLE_ARTICLE*sizeof(char)));
    }

    /*----------------------------------------*
    | Création du SMP de la file d'attente
    *-----------------------------------------*/
    int id_file;
    id_file = shmget(cle_smp_file, sizeof(int)*nb_archivistes, IPC_CREAT|IPC_EXCL|0660);
    if (id_file == -1){
        fprintf(stderr, "(Initial) Problème création SMP file\n");
    }

    /*----------------------------------------*
    | Attachement du SMP de la file d'attente 
    *-----------------------------------------*/
    int * adr_file = shmat(id_file, NULL, 0);
    if (adr_file == (void*)-1){
        fprintf(stderr, "(Initial) Problème attachement à l'EA du SMP File\n");
    }

    memset(adr_file, 0, sizeof(int)*nb_archivistes);

    /*----------------------------------------*
    | Création SMP nombre lecteurs (nombre de lecteurs actuels par thème)
    *-----------------------------------------*/
    int id_smp_nb_lecteurs;
    id_smp_nb_lecteurs = shmget(cle_smp_nb_lecteurs, sizeof(int) * nb_themes, IPC_CREAT|IPC_EXCL|0660);
    if (id_smp_nb_lecteurs == -1){
        fprintf(stderr, "(Initial) Problème création SMP nombre lecteurs\n");
    }

    /*----------------------------------------*
    | Attachement SMP nombre lecteurs
    *-----------------------------------------*/
    int * adr_smp_nb_lecteurs = shmat(id_smp_nb_lecteurs, NULL, 0);
    if (adr_smp_nb_lecteurs == (void*)-1){
        fprintf(stderr, "(Initial) Problème attachement à l'EA du SMP nb lecteurs\n");
    }
    memset(adr_smp_nb_lecteurs, 0, sizeof(int)*nb_themes);

    /*----------------------------------------*
    | Création SMP nombre écrivains (nombre de écrivains actuels par thème)
    *-----------------------------------------*/
    int id_smp_nb_ecrivains;
    id_smp_nb_ecrivains = shmget(cle_smp_nb_ecrivains, sizeof(int) * nb_themes, IPC_CREAT|IPC_EXCL|0660);
    if (id_smp_nb_ecrivains == -1){
        fprintf(stderr, "(Initial) Problème création SMP nombre lecteurs\n");
    }

    /*----------------------------------------*
    | Attachement SMP nombre écrivains
    *-----------------------------------------*/
    int * adr_smp_nb_ecrivains = shmat(id_smp_nb_ecrivains, NULL, 0);
    if (adr_smp_nb_ecrivains == (void*)-1){
        fprintf(stderr, "(Initial) Problème attachement à l'EA du SMP nb lecteurs\n");
    }
    memset(adr_smp_nb_ecrivains, 0, sizeof(int)*nb_themes);

    /*----------------------------------------*
    |                                         |
    | ~~~ FILE DE MESSAGES (Communication entre archivistes et journalistes)  ~~~
    |                                         |
    *-----------------------------------------*/

    /*----------------------------------------*
    | Création de la file de messages 
    *-----------------------------------------*/
    int file_message;
    file_message = msgget(cle_fm, IPC_CREAT|IPC_EXCL|0660);

    if (file_message == -1){
        fprintf(stderr, "(Initial) Problème création file de message\n");
        //exit(-1); pas d'exit car smp déja crées donc on veut les détruire a la fin du prog
    }

    /*----------------------------------------*
    |                                         |
    |          ~~~ SÉMAPHORES  ~~~
    |                                         |
    *-----------------------------------------*/

    /*----------------------------------------*
    | Création de l'ensemble de sémaphores -> 5 sémaphores différents utilisés pour chaque thème
    - (|ecriture|lecture|nb_lecteurs|nb_ecrivains|avant|)
    *-----------------------------------------*/
    int id_ens_sem = semget(cle_sem, nb_themes*5, IPC_CREAT|IPC_EXCL|0660);
    if (id_ens_sem == -1){
        fprintf(stderr, "(Initial) Problème création ensemble de sémaphores\n");
        //exit(-1); pas d'exit car smp et fm déja crées donc on veut les détruire a la fin du prog
    }

    /*----------------------------------------*
    | Initialisation ensemble de sémaphore (tout les sémaphores sont init a 1)
    *-----------------------------------------*/
    int init;
    for (int i=0; i < nb_themes*5; i++){
        init = semctl(id_ens_sem, i, SETVAL, 1);
        if (init == -1){
            fprintf(stderr, "(Initial) Problème Initialisaiton sémaphore %d\n", i);
        } 
    }

    /*----------------------------------------*
    | TEST EN REMPLISSANT LES THÈMES 
    *-----------------------------------------*/
    strcpy(adr_themes[0], "abcd");
    strcpy(adr_themes[1], "test");
    strcpy(adr_themes[1] + TAILLE_ARTICLE, "bapt");


    /*----------------------------------------*
    |                                         |
    | ~~~ LANCEMENT DES ARCHIVISTES  ~~~
    |                                         |
    *-----------------------------------------*/
    fprintf(stderr, "\n(Initial) Les archivistes se mettent au travail ! \n\n");

    pid_t pid;
    char * argv_archiviste[] = {"archiviste", NULL, NULL, NULL};
    argv_archiviste[1] = malloc(sizeof(int));
    argv_archiviste[2] = malloc(sizeof(int)*3); // *3 pour gcc car sinon warning débordement de tampon

    sprintf(argv_archiviste[2], "%d", nb_themes);

    for (int i=0; i<nb_archivistes; i++){
        pid = fork();
        sprintf(argv_archiviste[1], "%d", i);

        *(adr_file+i)=0;

        if (pid == -1){
            fprintf(stderr, "(Initial) Problème création archiviste (fils)\n");
            exit(-1);
        }

        if (pid == 0){
            execve("archiviste", argv_archiviste, NULL);
            exit(-1);
        }

        else{
            fils[pos_fils] = pid;
            pos_fils+=1;
        }
    }

    nb_fils = nb_archivistes;


    /*----------------------------------------*
    |                                         |
    | ~~~ LANCEMENT DES JOURNALISTES  ~~~
    |                                         |
    *-----------------------------------------*/
    char * argv_journaliste[]={"journaliste", NULL, NULL, NULL, NULL, NULL};
    argv_journaliste[1] = malloc(sizeof(int));
    argv_journaliste[2] = malloc(sizeof(int));
    argv_journaliste[3] = malloc(sizeof(int)); //idem pour gcc

    for (;;){ 
        nb_fils++;
        pid = fork();

        sleep(2);
        /*----------------------------------------*
        | Paramètres pour le Journaliste
        *-----------------------------------------*/
        sprintf(argv_journaliste[1], "%d", nb_archivistes);
        sprintf(argv_journaliste[2], "%c", action_journaliste());
        sprintf(argv_journaliste[3], "%d", theme_alea(nb_themes));

        /*----------------------------------------*
        | Si le journaliste veut faire une création ou une suppression
        | on lui donne un argument supplémentaire qui est le numéro de l'article
        *-----------------------------------------*/
        if (argv_journaliste[2][0] == 'c' || argv_journaliste[2][0] == 'e'){
            argv_journaliste[4] = malloc(sizeof(int));
            sprintf(argv_journaliste[4], "%d", article_alea());
        }

        if (pid == -1){
            fprintf(stderr, "(Initial) Problème création Journaliste (fils\n)");
            exit(-1);
        }

        if (pid == 0){
            execve("journaliste", argv_journaliste, NULL);
            exit(-1);
        }
 
        else{ //on le place dans le tableau des fils pour pouvoir le kill si jamais
            fils[pos_fils] = pid;
            pos_fils++;
        }

        if (code_arret == 1){ //si on recoit un signal d'arrêt
            destruction_fils();
            break;
        }

    }


    /*----------------------------------------*
    |                                         |
    |      ~~~ ATTENTE DES FILS ~~~
    |                                         |
    *-----------------------------------------*/
    int i=0;

    while(i<nb_fils){
        if (code_arret==1){
            destruction_fils();
        }
        waitpid(-1, 0, 0);
        i++; 
    }    

    fprintf(stderr, "\n");

    /*----------------------------------------*
    |                                         |
    |      ~~~ DESTRUCTION DES IPCS ~~~
    |                                         |
    *-----------------------------------------*/

    /*----------------------------------------*
    | Detachement et destruction des SMP (thèmes)
    *-----------------------------------------*/
    int detach_smp;
    int destruction_smp;
    for (int i=0; i<nb_themes; i++){
        detach_smp = shmdt(adr_themes[i]);
        if (detach_smp == -1){
            fprintf(stderr, "(Initial) Problème détachement SMP %d (Adresse invalide)\n",i);
        }

        destruction_smp = shmctl(id_themes[i], IPC_RMID, NULL);
        if (destruction_smp == -1){
            fprintf(stderr, "(Initial) Problème destruction SMP %d\n", i);
        }

    }

    /*----------------------------------------*
    | Détachement et destruction du SMP de la file
    *-----------------------------------------*/
    int detachement_file = shmdt(adr_file);
    if (detachement_file == -1){
        fprintf(stderr, "(Initial) Problème détachement SMP de la file\n");
    }

    int destruction_file = shmctl(id_file, IPC_RMID, NULL);
    if (destruction_file == -1){
        fprintf(stderr, "(Initial) Problème destruction SMP file\n");
    }

    /*----------------------------------------*
    | Détachement et destruction du SMP du nb de lecteurs
    *-----------------------------------------*/
    int detachement_nb_lecteurs = shmdt(adr_smp_nb_lecteurs);
    if (detachement_nb_lecteurs == -1){
        fprintf(stderr, "(Initial) Problème détachement SMP du nb de lecteurs\n");
    }

    int destruction_nb_lecteurs = shmctl(id_smp_nb_lecteurs, IPC_RMID, NULL);
    if (destruction_nb_lecteurs == -1){
        fprintf(stderr, "(Initial) Problème destruction SMP du nb de lecteurs\n");
    }

    /*----------------------------------------*
    | Détachement et destruction du SMP du nb d'écrivains
    *-----------------------------------------*/
    int detachement_nb_ecrivains = shmdt(adr_smp_nb_ecrivains);
    if (detachement_nb_ecrivains == -1){
        fprintf(stderr, "(Initial) Problème détachement SMP du nb d'écrivains\n");
    }

    int destruction_nb_ecrivains = shmctl(id_smp_nb_ecrivains, IPC_RMID, NULL);
    if (destruction_nb_ecrivains == -1){
        fprintf(stderr, "(Initial) Problème destruction SMP du nb d'écrivains\n");
    }

    /*----------------------------------------*
    | Destruction de la file de messages 
    *-----------------------------------------*/
    int destruction_fm = msgctl(file_message, IPC_RMID, NULL);
    if (destruction_fm == -1){
        fprintf(stderr, "(Initial) Problème destruction file de messages\n");
    }


    /*----------------------------------------*
    | Destruction ensemble de sémaphores
    *-----------------------------------------*/
    int destruction_sem = semctl(id_ens_sem, 0, IPC_RMID, NULL);
    if (destruction_sem == -1){
        fprintf(stderr, "(Initial) Problème destruction sémaphore\n");
    }


    exit(0);

}
