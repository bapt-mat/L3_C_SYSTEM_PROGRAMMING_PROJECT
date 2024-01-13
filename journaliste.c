#include "types.h"
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
| Arrêt du programme 
*-----------------------------------------*/
void arret(){
    fprintf(stderr, "(Journaliste %d) a reçu un signal d'arrêt\n", getpid());
    exit(-1);
}

/*----------------------------------------*
| Affichage de la file d'attente des Archivistes
*-----------------------------------------*/
void affichage_file_attente(int * adr_file, int nb_archivistes){
    couleur(CYAN);fprintf(stdout, "======================================\n");couleur(REINIT);
    couleur(CYAN); fprintf(stdout, "||");couleur(REINIT);
    couleur(ROUGE); fprintf(stdout, "         FILE D'ATTENTE");couleur(REINIT);
    couleur(CYAN); fprintf(stdout, "          ||\n");couleur(REINIT);
    couleur(CYAN); fprintf(stdout, "======================================\n");couleur(REINIT);
   
    for (int i=0; i<nb_archivistes; i++){
       couleur(CYAN); fprintf(stdout, "|");couleur(REINIT);
       couleur(ROUGE); fprintf(stdout, "Archiviste n°%d ", i);couleur(REINIT);
       couleur(CYAN);fprintf(stdout, "||");couleur(REINIT);
       couleur(ROUGE);  fprintf(stdout, "Nombre de tache : %d",*(adr_file+i));couleur(REINIT);
       couleur(CYAN); fprintf(stdout, "|\n");couleur(REINIT);
    }
    couleur(CYAN);fprintf(stdout, "======================================\n");couleur(REINIT);

    fprintf(stderr, "\n");
}

/*----------------------------------------*
| Choisi l'archiviste le moins occupé
*-----------------------------------------*/
int choix_moins_taf(int * adr_file, int nb_archivistes) {
    int min = *adr_file;
    int num_archiviste = 0;
    for (int i=1; i < nb_archivistes; i++){
        if (*(adr_file+i) < min){
            min = *(adr_file+i);
            num_archiviste = i;
        }
    }    
    return num_archiviste;
}


/*----------------------------------------*
| Texte aléatoire
*-----------------------------------------*/
char * texte_aleatoire(){ 
    char * txt = (char*)malloc(5*sizeof(char));
    if (txt==NULL){
        fprintf(stderr,"(Journaliste %d)Erreur allocation mémoire txt\n", getpid());
        exit(-1);
    }
    for (int i=0;i<4;i++){
        txt[i]='a'+rand()% + 26 ;
    }
    txt[4]='\0';

    return txt;
}



int main(int argc, char * argv[]){
    srand(getpid());
    /*----------------------------------------*
    |                                         |
    |   ~~~ VARIABLES ET ARGUMENTS ~~~
    |                                         |
    *-----------------------------------------*/
    if (argc<4){
        fprintf(stderr, "(Journaliste %d) Problème nombre arguments\n", getpid());
        exit(-1);
    }


    int pid_journaliste = getpid();
    int nb_archivistes = atoi(argv[1]);
    char action_journaliste = argv[2][0];
    int num_theme = atoi(argv[3]);
    int num_article;

    requete_t requete_envoyer;
    reponse_t reponse_recu;

    /*----------------------------------------*
    |                                         |
    |      ~~~ GESTION DES SIGNAUX ~~~
    |                                         |
    *-----------------------------------------*/
    mon_sigaction(SIGINT, arret);

    /*----------------------------------------*
    | Si on a 5 arguments -> cela veut dire que c'est une consultation ou suppression
    *-----------------------------------------*/
    if (argc == 5){
        num_article = atoi(argv[4]);
        requete_envoyer.num_article = num_article;
    }
    /*----------------------------------------*
    | Sinon c'est une publication 
    *-----------------------------------------*/
    if (action_journaliste == 'p'){
        strcpy(requete_envoyer.texte_article, texte_aleatoire());
    }

    FILE * fichier_cle;
    key_t cle_fm;
    key_t cle_smp_file;

    /*----------------------------------------*
    |                                         |
    |           ~~~ FICHIER CLÉ ~~~
    |                                         |
    *-----------------------------------------*/
    fichier_cle = fopen(FICHIER_CLE, "r");
    if (fichier_cle == NULL){
        fprintf(stderr, "(Journaliste %d) Problème récupération fichier clé\n", pid_journaliste);
        exit(-1);
    }

    /*----------------------------------------*
    |                                         |
    |             ~~~ CLÉS ~~~
    |                                         |
    *-----------------------------------------*/
    /*----------------------------------------*
    | Clé pour la file de message
    *-----------------------------------------*/
    cle_fm = ftok(FICHIER_CLE, code_cle_fm);
    if (cle_fm == -1){
        fprintf(stderr, "(Journaliste %d) Problème création clé file de message\n", pid_journaliste);
        exit(-1);
    }

    /*----------------------------------------*
    | Clé pour le SMP de la file d'attente
    *-----------------------------------------*/
    cle_smp_file = ftok(FICHIER_CLE, code_cle_smp_file);
    if (cle_smp_file == -1){
        fprintf(stderr, "(Journaliste %d) Problème création clé SMP file d'attente\n", pid_journaliste);
        exit(-1);
    }

    /*----------------------------------------*
    |                                         |
    |      ~~~ RÉCUPÉRATION DES IPCS ~~~
    |                                         |
    *-----------------------------------------*/
    /*----------------------------------------*
    | Récupération de la file de messages (Communication entre archivistes et journalistes)
    *-----------------------------------------*/
    int file_message;
    file_message = msgget(cle_fm, 0);
    if (file_message == -1){
        fprintf(stderr, "(Journaliste %d) Problème récupération file de messages\n", pid_journaliste);
    }

    /*----------------------------------------*
    | Récupération du SMP de la file d'attente
    *-----------------------------------------*/
    int id_file = shmget(cle_smp_file, 0, 0);
    if (id_file == -1){
        fprintf(stderr, "(Journaliste %d) Problème récupération SMP file\n", pid_journaliste);
    }

    /*----------------------------------------*
    | Attachement du SMP de la file d'attente
    *-----------------------------------------*/
    int * adr_file = shmat(id_file, NULL, 0);
    if (adr_file == (void*)-1){
        fprintf(stderr, "(Journaliste %d) Problème récupération attachement à l'EA du SMP file\n", pid_journaliste);
    }

    /*----------------------------------------*
    | Envoi du message dans la file de messages
    *-----------------------------------------*/
    int num_archiviste_moins_taf = choix_moins_taf(adr_file, nb_archivistes); //code pour s'adresser au bon archiviste 
    requete_envoyer.type = ARCHIVE + num_archiviste_moins_taf;
    requete_envoyer.nature = action_journaliste;
    requete_envoyer.theme = num_theme;
    requete_envoyer.expediteur = pid_journaliste;

    /*----------------------------------------*
    | Envoi de la requête 
    *-----------------------------------------*/
    //fprintf(stderr, "(Journaliste %d) Envoie sa requête : %c -> Archiviste n°%d \n", pid_journaliste, requete_envoyer.nature, num_archiviste_moins_taf);
    int envoi = msgsnd(file_message, &requete_envoyer, sizeof(requete_t)-sizeof(long), IPC_NOWAIT);
    if (envoi == -1){
        fprintf(stderr, "(Journaliste %d) Problème envoie requete\n", pid_journaliste);
    }

    /*----------------------------------------*
    | Incrémente la file d'attente de l'archiviste auquel on demande de travailler
    *-----------------------------------------*/
    *(adr_file+num_archiviste_moins_taf) += 1;

    /*----------------------------------------*
    | Attente de la réponse à sa requête (de type son pid pour récupérer le message qui lui est adressé)
    *-----------------------------------------*/
    int reponse = msgrcv(file_message, &reponse_recu, sizeof(reponse_t)-sizeof(long), pid_journaliste, 0);
    if (reponse == -1){
        fprintf(stderr, "(Journaliste %d) Problème récupération réponse\n", pid_journaliste);
    }

    /*----------------------------------------*
    | Affichage de la réponse
    *-----------------------------------------*/
    
    if(reponse_recu.requete_satisfaite == 1){
        if (requete_envoyer.nature == 'c'){
            couleur(BLEU);fprintf(stdout, "\n(Journaliste %d) Réponse : Archiviste %d Lecture de l'article %d du thème %d : '%s'\n", pid_journaliste, num_archiviste_moins_taf, num_article, num_theme, reponse_recu.texte_article_reponse);couleur(REINIT);
        }
        if (requete_envoyer.nature == 'p'){
            couleur(BLEU);fprintf(stdout, "\n(Journaliste %d) Réponse : Archiviste %d a bien ajouté l'article '%s' au thème %d\n", pid_journaliste, num_archiviste_moins_taf, requete_envoyer.texte_article, num_theme);couleur(REINIT);
            affichage_file_attente(adr_file, nb_archivistes);
        }
        if (requete_envoyer.nature == 'e'){
            couleur(BLEU);fprintf(stdout, "\n(Journaliste %d) Réponse : Archiviste %d a bien supprimé l'article %d du thème %d\n", pid_journaliste, num_archiviste_moins_taf,num_article, num_theme);couleur(REINIT);
            affichage_file_attente(adr_file, nb_archivistes);
        }
    }
    else{
        if (requete_envoyer.nature == 'c'){
            couleur(BLEU);fprintf(stdout, "\n(Journaliste %d) Réponse : Archiviste %d Lecture de l'article %d du thème %d IMPOSSIBLE\n", pid_journaliste, num_archiviste_moins_taf, num_article, num_theme);couleur(REINIT);
        }
        if (requete_envoyer.nature == 'p'){
            couleur(BLEU);fprintf(stdout, "\n(Journaliste %d) Réponse : Archiviste %d N'A PAS PU ajouté l'article '%s' au thème %d\n", pid_journaliste, num_archiviste_moins_taf, requete_envoyer.texte_article, num_theme);couleur(REINIT);
            affichage_file_attente(adr_file, nb_archivistes);
        }
        if (requete_envoyer.nature == 'e'){
            couleur(BLEU);fprintf(stdout, "\n(Journaliste %d) Réponse : Archiviste %d N'A PAS PU supprimé l'article %d du thème %d\n", pid_journaliste, num_archiviste_moins_taf, num_article, num_theme);couleur(REINIT);
            affichage_file_attente(adr_file, nb_archivistes);
        }
    }

    /*----------------------------------------*
    | Décremente de 1 la file d'attente de l'archiviste requisitionné car travail terminé !
    *-----------------------------------------*/
    *(adr_file+num_archiviste_moins_taf) -= 1;


    /*----------------------------------------*
    |                                         |
    |      ~~~ DESTRUCTION DES IPCS ~~~
    |                                         |
    *-----------------------------------------*/
    /*----------------------------------------*
    | Détachement du SMP de la file
    *-----------------------------------------*/
    int detachement_file = shmdt(adr_file);
    if (detachement_file == -1){
        fprintf(stderr, "(Journaliste %d) Problème détachement SMP de la file\n", pid_journaliste);
    }

    exit(0);
}