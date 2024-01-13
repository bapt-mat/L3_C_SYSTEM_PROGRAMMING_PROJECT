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
    fprintf(stderr, "(Archiviste %d) a reçu un signal d'arrêt\n", getpid());
    exit(-1);
}


/*----------------------------------------*
| Affichage contenu des piles de thèmes
*-----------------------------------------*/
void affichage_theme(char * adr_theme, int num_theme){
    couleur(VERT);fprintf(stdout, "THEME [%d]\n", num_theme);couleur(REINIT);
    for (int i=0; i<TAILLE_ARTICLE*MAX_ARTICLE; i++){
        couleur(VERT);fprintf(stderr, "%c |", adr_theme[i]);couleur(REINIT);
    }
    fprintf(stderr, "\n");
}

/*----------------------------------------*
| Simulation du travail
*-----------------------------------------*/
void travaille() {
    int i;
    struct timespec delay;
    delay.tv_sec = 0;        
    delay.tv_nsec = 300000000;  

    for (i = 0; i < 20; i++) {
        nanosleep(&delay, NULL);
        //fprintf(stderr, "Archiviste %d travaille...\n", getpid());
    }
    fprintf(stderr, "\n");
}

/*----------------------------------------*
| Vérifie si l'article num_article existe dans la chaine
*-----------------------------------------*/
int existe (char *chaine_char, int num_article){
    int deplacement_article = num_article*TAILLE_ARTICLE;

    if (chaine_char[deplacement_article] != '\0'){
        return 1;
    }else{
        return 0;
    }
}

/*--------------------------------------*
| Place disponible dans le SMP du thème
*---------------------------------------*/
int place_disponible(char * adr_theme){     
    int max = TAILLE_ARTICLE * MAX_ARTICLE;
    int i ;
    for (i = 0 ; i<max; i=i+TAILLE_ARTICLE ){
        if (adr_theme[i] == '\0'){
            return i;
        }
    }
    return -1;
}

/*----------------------------------------*
| Fonction de récupération d'un sémaphore
*-----------------------------------------*/
int prendre_semaphore(int id_ens, int n, int num_sem){
    struct sembuf op = {num_sem, -n, SEM_UNDO};

    return semop(id_ens, &op, 1);
}

/*----------------------------------------*
| Fonction pour redonner le sémaphore
*-----------------------------------------*/
int vendre_semaphore(int id_ens, int n, int num_sem){
    struct sembuf op = {num_sem, n, SEM_UNDO};

    return semop(id_ens, &op, 1);
}

int main(int argc, char *argv[]){
    srand(time(NULL));
    /*----------------------------------------*
    |                                         |
    |    ~~~ VARIABLES ET ARGUMENTS ~~~
    |                                         |
    *-----------------------------------------*/

    if (argc != 3){
        fprintf(stderr,"(Archiviste %d) Problème arguments archiviste\n", getpid());
        exit(-1);
    }

    int numero_ordre = atoi(argv[1]);
    int nb_themes = atoi(argv[2]);
    int pid_archiviste = getpid();

    requete_t requete_recu;
    reponse_t reponse_envoyer;

    FILE * fichier_cle;
    key_t cle_theme[nb_themes];
    key_t cle_fm;
    key_t cle_sem;
    key_t cle_smp_nb_lecteurs;
    key_t cle_smp_nb_ecrivains;

    /*----------------------------------------*
    |                                         |
    |      ~~~ GESTION DES SIGNAUX  ~~~
    |                                         |
    *-----------------------------------------*/
    mon_sigaction(SIGINT, arret);

    /*----------------------------------------*
    *                                         *
    *          ~~~ FICHIER CLÉ~~~             
    *                                         *
    *-----------------------------------------*/
    fichier_cle = fopen(FICHIER_CLE, "r");
    if (fichier_cle == NULL){
        fprintf(stderr, "(Archiviste %d) Problème récupération fichier clé\n", pid_archiviste);
        exit(-1);
    }

    /*----------------------------------------*
    *                                         *
    *             ~~~ CLÉS ~~~ 
    *                                         *
    *-----------------------------------------*/

    /*----------------------------------------*
    | Clés pour les SMP des thèmes
    *-----------------------------------------*/
    for (int i=0; i<nb_themes; i++){
        cle_theme[i] = ftok(FICHIER_CLE, i);
        if (cle_theme[i] == -1){
            fprintf(stderr, "(Archiviste %d) Problème création clé SMP\n", pid_archiviste);
            exit(-1);
        }
    }

    /*----------------------------------------*
    | Clé pour le SMP des nombres de lecteurs
    *-----------------------------------------*/
    cle_smp_nb_lecteurs = ftok(FICHIER_CLE, code_cle_smp_nb_lecteurs);
    if (cle_smp_nb_lecteurs == -1){
        fprintf(stderr, "(Archiviste %d) Problème création clé SMP nombre lecteurs\n", pid_archiviste);
        exit(-1);
    }

    /*----------------------------------------*
    | Clé pour le SMP des nombres d'écrivains
    *-----------------------------------------*/
    cle_smp_nb_ecrivains = ftok(FICHIER_CLE, code_cle_smp_nb_ecrivains);
    if (cle_smp_nb_ecrivains == -1){
        fprintf(stderr, "(Archiviste %d) Problème création clé SMP nombre écrivains\n", pid_archiviste);
        exit(-1);
    }

    /*----------------------------------------*
    | Clé pour la file de message
    *-----------------------------------------*/ 
    cle_fm = ftok(FICHIER_CLE, code_cle_fm);
    if (cle_fm == -1){
        fprintf(stderr, "(Archiviste %d) Problème création clé file de message\n", pid_archiviste);
        exit(-1);
    }

    /*----------------------------------------*
    | Clés pour l'ensemble de sémaphores 
    *-----------------------------------------*/
    cle_sem = ftok(FICHIER_CLE, code_cle_sem);
    if (cle_sem == -1){
        fprintf(stderr, "(Archiviste %d) Problème création clé sémaphore\n", pid_archiviste);
        exit(-1);
    }

    /*----------------------------------------*
    |                                         |
    |     ~~~ RÉCUPÉRATION DES IPCS ~~~        |
    |                                         |
    *----------------------------------------*/
    /*----------------------------------------*
    |                                         |
    | RÉCUP. DES SEGMENTS DE MÉMOIRES PARTAGÉES (THEMES)
    |                                         |
    *----------------------------------------*/
    int id_themes[nb_themes];
    for (int i=0; i<nb_themes; i++){
        id_themes[i] = shmget(cle_theme[i], 0, 0);
        if (id_themes[i] == -1){
            fprintf(stderr, "(Archiviste %d) Problème récupération id SMP %d\n", pid_archiviste, i);
        }
    }

    /*----------------------------------------*
    * Attachement des SMP (Thèmes)
    *-----------------------------------------*/
    char * adr_themes[nb_themes]; // Tableau d'adresse des SMP (thèmes)
    for (int i=0; i<nb_themes; i++){
        adr_themes[i] = shmat(id_themes[i], NULL, 0);
        if (adr_themes[i] == (void*)-1){
            fprintf(stderr, "(Archiviste %d) Problème attachement a l'EA du SMP n°%d\n", pid_archiviste, i);
            exit(-1);
        }
    }

    /*----------------------------------------*
    | Récupération SMP nombre lecteurs (nombre de lecteurs actuels par thème)
    *-----------------------------------------*/
    int id_smp_nb_lecteurs;
    id_smp_nb_lecteurs = shmget(cle_smp_nb_lecteurs, 0, 0);
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

    /*----------------------------------------*
    | Récupération SMP nombre écrivains (nombre de écrivains actuels par thème)
    *-----------------------------------------*/
    int id_smp_nb_ecrivains;
    id_smp_nb_ecrivains = shmget(cle_smp_nb_ecrivains, 0, 0);
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

    /*----------------------------------------*
    |                                         |
    | ~~~ RÉCUPÉRATION DE LA FILE DE MESSAGES (Communication entre archivistes et journalistes) ~~~
    |                                         |
    *-----------------------------------------*/
    int file_message;
    file_message = msgget(cle_fm, 0);
    if (file_message == -1){
        fprintf(stderr, "(Archiviste %d) Problème récupération file de messages\n", pid_archiviste);
    }

    /*----------------------------------------*
    |                                         |
    | ~~~ RÉCUPÉRATION DE L'ENSEMBLE DE SÉMAPHORES ~~~
    |                                         |
    *-----------------------------------------*/
    int id_ens_sem = semget(cle_sem, 0, 0);
    if (id_ens_sem == -1){
        fprintf(stderr, "(Archiviste %d) Problème récupération ensemble de sémaphores\n", pid_archiviste);
    }

    /*----------------------------------------*
    |                                         |
    | ~~~ BOUCLE LECTURE FILE DE MESSAGES ~~~
    |                                         |
    *-----------------------------------------*/

    fprintf(stderr, "(Archiviste %d) En activité. N°ordre : %d.\n", getpid(), numero_ordre);

    int reception;
    int p;
    int v;
    int num_article;

    for (;;){
        /*----------------------------------------*
        | Attente d'un message dans la file de message (ARCHIVE+numero_ordre -> code de l'archiviste pour récup les messages qui lui sont adressés)
        *-----------------------------------------*/
        reception = msgrcv(file_message, &requete_recu, sizeof(requete_t), ARCHIVE+numero_ordre, 0);

        if (reception !=1){

            if (requete_recu.nature == 'c'){
                couleur(VERT);fprintf(stdout, "\n(Archiviste %d - n°%d) Requête : Journaliste %d veut consulter l'article %d du thème %d\n", pid_archiviste, numero_ordre, requete_recu.expediteur, requete_recu.num_article, requete_recu.theme);couleur(REINIT);
            }
            if (requete_recu.nature == 'p'){
                couleur(VERT);fprintf(stdout, "\n(Archiviste %d - n°%d) Requête : Journaliste %d veut écrire l'article '%s' dans le thème %d\n", pid_archiviste, numero_ordre, requete_recu.expediteur, requete_recu.texte_article, requete_recu.theme);couleur(REINIT);
            }
            if (requete_recu.nature == 'e'){
                couleur(VERT);fprintf(stdout, "\n(Archiviste %d - n°%d) Requête : Journaliste %d veut effacer l'article %d dans le thème %d\n", pid_archiviste, numero_ordre, requete_recu.expediteur, requete_recu.num_article, requete_recu.theme);couleur(REINIT);
            }
            affichage_theme(adr_themes[requete_recu.theme], requete_recu.theme);
        }       

        int num_theme = requete_recu.theme;

        reponse_envoyer.type = requete_recu.expediteur;

        /*----------------------------------------*
        | Switch sur la nature de la requete 
        *-----------------------------------------*/
        switch (requete_recu.nature){
            /*----------------------------------------*
            |                                         |
            | ~~~ CONSULTATION D'UN ARTICLE ~~~
            |                                         |
            *-----------------------------------------*/
            case 'c':          
                
                num_article = requete_recu.num_article;

                /*----------------------------------------*
                | P(avant)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_AVANT*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore AVANT\n", pid_archiviste);
                }
                /*----------------------------------------*
                | P(Lecture)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_LECTURE*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore LECTURE\n", pid_archiviste);
                }
                /*----------------------------------------*
                | P(mutex_nb_lecteurs)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_LECTEURS*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB LECTEURS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | nb_lecteurs ++
                | si nb_lecteurs == 1 -> P(ecriture)
                *-----------------------------------------*/
                *(adr_smp_nb_lecteurs+num_theme) += 1;

                if (*(adr_smp_nb_lecteurs+num_theme) == 1){
                    /*----------------------------------------*
                    | P(Ecriture)
                    *-----------------------------------------*/
                    p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_ECRITURE*nb_themes));
                    if (p==-1){
                        fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB ECRIVAINS\n", pid_archiviste);
                    }
                }

                /*----------------------------------------*
                | V(mutex_nb_lecteurs)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_LECTEURS*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore NB LECTEURS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | V(Lecture)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_LECTURE*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore LECTURE\n", pid_archiviste);
                }
                /*----------------------------------------*
                | V(avant)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_AVANT*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore AVANT\n", pid_archiviste);
                }

                couleur(VERT);fprintf(stdout, "(Archiviste %d) cherche l'article %d dans ses archives du thème %d ...", pid_archiviste, num_article, num_theme);couleur(REINIT);

                /*----------------------------------------*
                | RECHERCHE DE L'ARTICLE
                *-----------------------------------------*/
                /*---------------------------------------*
                | Simule l'exécution d'un travail
                *---------------------------------------*/
                travaille();

                /*----------------------------------------*
                | L'archiviste cherche l'article et le renvoie si il existe
                *-----------------------------------------*/
                if(existe(adr_themes[num_theme], num_article) == 1){
                    strcpy(reponse_envoyer.texte_article_reponse, adr_themes[num_theme]+(num_article*TAILLE_ARTICLE));
                    reponse_envoyer.requete_satisfaite = 1;
                }
                else{   //Si l'article n'existe pas
                    reponse_envoyer.requete_satisfaite = 0;
                }

                /*----------------------------------------*
                | FIN DE LA RECHERCHE
                *-----------------------------------------*/

                /*----------------------------------------*
                | P(mutex_nb_lecteurs)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_LECTEURS*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB LECTEURS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | nb_lecteurs --;
                | si nb_lecteurs == 0 -> V(ecriture)
                *-----------------------------------------*/
                *(adr_smp_nb_lecteurs+num_theme) -= 1;
                if (*(adr_smp_nb_lecteurs+num_theme) == 0){
                    /*----------------------------------------*
                    | V(Ecriture)
                    *-----------------------------------------*/
                    v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_ECRITURE*nb_themes));
                    if (v==-1){
                        fprintf(stderr, "(Archiviste %d) Problème vente sémaphore ECRITURE\n", pid_archiviste);
                    }
                }
                /*----------------------------------------*
                | V(mutex_nb_lecteurs)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_LECTEURS*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore NB LECTEURS\n", pid_archiviste);
                }

                break;

            /*----------------------------------------*
            |                                         |
            | ~~~ PUBLICATION D'UN ARTICLE ~~~
            |                                         |
            *-----------------------------------------*/
            case 'p':       
                /*----------------------------------------*
                | P(mutex_nb_ecrivains)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB ECRIVAINS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | nb_ecrivains ++;
                | si nb ecrivains == 1 -> P(Lecture)
                *-----------------------------------------*/
                *(adr_smp_nb_ecrivains+num_theme) +=1;
                if (*(adr_smp_nb_ecrivains+num_theme) == 1){
                    /*----------------------------------------*
                    | P(Lecture)
                    *-----------------------------------------*/
                    p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_LECTURE*nb_themes));
                    if (p==-1){
                        fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore LECTURE\n", pid_archiviste);
                    }
                }
                /*----------------------------------------*
                | V(mutex_nb_ecrivains)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore NB ECRIVAINS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | P(ecriture) -> L'archiviste prend le sémaphore du thème 
                *-----------------------------------------*/  
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_ECRITURE*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore\n", pid_archiviste);
                }
                else{
                    couleur(VERT);fprintf(stdout, "\n(Archiviste %d) travaille dans les archives du thème %d (Publication article)\n", pid_archiviste, num_theme);couleur(REINIT);
                }

                /*----------------------------------------*
                | DEBUT SECTION CRITIQUE
                *-----------------------------------------*/
               
                /*---------------------------------------*
                | Simule l'exécution d'un travail
                *---------------------------------------*/
                travaille();

                /*----------------------------------------*
                * Prépare le texte a renvoyer
                *-----------------------------------------*/
                char texte_publication[5];
                strcpy(texte_publication, requete_recu.texte_article);

                /*----------------------------------------*
                *  Ecriture dans le segment de mémoire partagée du thème si il reste de la place
                *-----------------------------------------*/
                int place_article = place_disponible(adr_themes[num_theme]);
                if (place_article != -1){ 
                    strcpy(adr_themes[num_theme]+place_article, texte_publication);
                    reponse_envoyer.requete_satisfaite = 1;
                    affichage_theme(adr_themes[num_theme], num_theme);
                }
                else{                     
                    reponse_envoyer.requete_satisfaite = 0;
                    affichage_theme(adr_themes[num_theme], num_theme);
                }

                /*----------------------------------------*
                | FIN SECTION CRITIQUE
                *-----------------------------------------*/

                /*----------------------------------------*
                * L'archiviste redonne le sémaphore du thème -> V(ecriture)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_ECRITURE*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore\n", pid_archiviste);
                }
                else{
                    couleur(VERT);fprintf(stdout, "(Archiviste %d) sors des archives du thème %d (Publication article)\n\n", pid_archiviste, num_theme);couleur(REINIT);
                }
                /*----------------------------------------*
                | P(mutex_nb_ecrivains)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB ECRIVAINS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | nb_ecrivains --
                | si nb_ecrivains == 0 -> V(lecture);
                *-----------------------------------------*/
                *(adr_smp_nb_ecrivains+num_theme) -=1;
                if (*(adr_smp_nb_ecrivains+num_theme) == 0){
                    /*----------------------------------------*
                    | V(Lecture)
                    *-----------------------------------------*/
                    v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_LECTURE*nb_themes));
                    if (v==-1){
                        fprintf(stderr, "(Archiviste %d) Problème vente sémaphore LECTURE\n", pid_archiviste);
                    }
                }
                /*----------------------------------------*
                | V(mutex_nb_ecrivains)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore NB ECRIVAINS\n", pid_archiviste);
                }

                break;

            /*----------------------------------------*
            |                                         |
            | ~~~ EFFACEMENT D'UN ARTICLE ~~~
            |                                         |
            *-----------------------------------------*/
            case 'e':
                /*----------------------------------------*
                | P(mutex_nb_ecrivains)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB ECRIVAINS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | nb_ecrivains ++;
                | si nb ecrivains == 1 -> P(Lecture)
                *-----------------------------------------*/
                *(adr_smp_nb_ecrivains+num_theme) +=1;
                if (*(adr_smp_nb_ecrivains+num_theme) == 1){
                    /*----------------------------------------*
                    | P(Lecture)
                    *-----------------------------------------*/
                    p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_LECTURE*nb_themes));
                    if (p==-1){
                        fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore LECTURE\n", pid_archiviste);
                    }
                }
                /*----------------------------------------*
                | V(mutex_nb_ecrivains)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore NB ECRIVAINS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | P(ecriture) -> L'archiviste prend le sémaphore du thème 
                *-----------------------------------------*/  
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_ECRITURE*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore\n", pid_archiviste);
                }
                else{
                    couleur(VERT);fprintf(stdout, "\n(Archiviste %d) travaille dans les archives du thème %d (Effacement article)\n", pid_archiviste, num_theme);couleur(REINIT);
                }

                /*----------------------------------------*
                | DEBUT SECTION CRITIQUE
                *-----------------------------------------*/

                /*----------------------------------------*
                | Simule l'exécution d'un travail
                *-----------------------------------------*/
                travaille();
                
                num_article = requete_recu.num_article;

                /*----------------------------------------*
                | Effacement de l'article du segment de mémoire partagée
                *-----------------------------------------*/
                if (existe(adr_themes[num_theme], num_article) == 1){ 
                    memset(adr_themes[num_theme]+(num_article*TAILLE_ARTICLE), 0, TAILLE_ARTICLE);
                    reponse_envoyer.requete_satisfaite = 1;
                }
                else{                      
                    reponse_envoyer.requete_satisfaite = 0;
                }

                /*----------------------------------------*
                | FIN SECTION CRITIQUE
                *-----------------------------------------*/

                /*----------------------------------------*
                | L'archiviste redonne le sémaphore du thème -> V(ecriture)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_ECRITURE*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore\n", pid_archiviste);
                }
                else{
                    couleur(VERT);fprintf(stderr, "(Archiviste %d) sors des archives du thème %d (Effacement article)\n\n", pid_archiviste, num_theme);couleur(REINIT);
                }
                /*----------------------------------------*
                | P(mutex_nb_ecrivains)
                *-----------------------------------------*/
                p = prendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (p==-1){
                    fprintf(stderr, "(Archiviste %d) Problème récupération sémaphore NB ECRIVAINS\n", pid_archiviste);
                }
                /*----------------------------------------*
                | nb_ecrivains --
                | si nb_ecrivains == 0 -> V(lecture);
                *-----------------------------------------*/
                *(adr_smp_nb_ecrivains+num_theme) -=1;
                if (*(adr_smp_nb_ecrivains+num_theme) == 0){
                    /*----------------------------------------*
                    | V(Lecture)
                    *-----------------------------------------*/
                    v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_LECTURE*nb_themes));
                    if (v==-1){
                        fprintf(stderr, "(Archiviste %d) Problème vente sémaphore LECTURE\n", pid_archiviste);
                    }
                }
                /*----------------------------------------*
                | V(mutex_nb_ecrivains)
                *-----------------------------------------*/
                v = vendre_semaphore(id_ens_sem, 1, num_theme+(DEPL_NB_ECRIVAINS*nb_themes));
                if (v==-1){
                    fprintf(stderr, "(Archiviste %d) Problème vente sémaphore NB ECRIVAINS\n", pid_archiviste);
                }

                break;

            default:
                fprintf(stderr,"(Archiviste %d ERROR) Problème action à réaliser \n", pid_archiviste);
                exit(-1);
                break;
        }

        /*----------------------------------------*
        | Envoi de la réponse (de type du pid de l'expéditeur pour adresser le message au bon processus)
        *-----------------------------------------*/
        int envoi = msgsnd(file_message, &reponse_envoyer, sizeof(reponse_t)-sizeof(long), IPC_NOWAIT);
        if (envoi == -1){
            fprintf(stderr, "(Archiviste %d) Problème envoi réponse au journaliste\n", pid_archiviste);
        }

    }   


    /*----------------------------------------*
    |                                         |
    | ~~~ DESTRUCTIION DES IPCS ~~~
    |                                         |
    *-----------------------------------------*/
    /*----------------------------------------*
    | Detachement des SMP (thèmes)
    *-----------------------------------------*/
    int detach_smp;
    for (int i=0; i<nb_themes; i++){
        detach_smp = shmdt(adr_themes[i]);
        if (detach_smp == -1){
            fprintf(stderr, "(Archiviste %d)  Problème détachement SMP %d (Adresse invalide)\n",pid_archiviste,i);
        }

        fprintf(stderr, "(Archiviste %d) Detachement du theme (SMP) %d\n", pid_archiviste, i);
    }

    /*----------------------------------------*
    | Détachement du SMP du nb de lecteurs
    *-----------------------------------------*/
    int detachement_nb_lecteurs = shmdt(adr_smp_nb_lecteurs);
    if (detachement_nb_lecteurs == -1){
        fprintf(stderr, "(Initial) Problème détachement SMP du nb de lecteurs\n");
    }
    else{
        fprintf(stderr, "(Initial) Détachement SMP nb lecteurs\n");
    }

    /*----------------------------------------*
    | Détachement du SMP du nb d'écrivains
    *-----------------------------------------*/
    int detachement_nb_ecrivains = shmdt(adr_smp_nb_ecrivains);
    if (detachement_nb_ecrivains == -1){
        fprintf(stderr, "(Initial) Problème détachement SMP du nb d'écrivains\n");
    }
    else{
        fprintf(stderr, "(Initial) Détachement SMP nb ecrivains\n");
    }

    exit(EXIT_SUCCESS);
}