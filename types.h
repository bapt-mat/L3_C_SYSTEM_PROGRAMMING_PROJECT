#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#define FICHIER_CLE "cle.serv"

#define MAX_ARTICLE 10
#define TAILLE_ARTICLE 5 // 4 (char) + 1 (\0 fin de chaine de caractères)

/*----------------------------------------*
| Codes clés
*-----------------------------------------*/
#define code_cle_fm 8888
#define code_cle_sem 9999
#define code_cle_smp_file 7777
#define code_cle_smp_nb_lecteurs 8989
#define code_cle_smp_nb_ecrivains 7878

/*----------------------------------------*
| Ensemble de sémaphores (déplacement pour parcourir l'ensemble de sémaphore)
*-----------------------------------------*/

#define DEPL_ECRITURE 0 //pour la forme
#define DEPL_LECTURE 1
#define DEPL_NB_LECTEURS 2
#define DEPL_NB_ECRIVAINS 3
#define DEPL_AVANT 4

/*----------------------------------------*
| Structure pour la file de message
*-----------------------------------------*/

#define ARCHIVE 762001

typedef struct
{   
    long type;
    char nature;
    int theme;
    int num_article;
    char texte_article[5];
    pid_t expediteur;
} 
requete_t;

typedef struct
{   
    long type;
    int requete_satisfaite;
    char texte_article_reponse[5];
} 
reponse_t;

#define MAX_FILS 4096 //a voir si on laisse max_fils en constante 
typedef pid_t Tableau_Fils[MAX_FILS];

/*----------------------------------------*
| Couleurs
*-----------------------------------------*/

#define couleur(param) printf("\033[%sm",param)

#define NOIR  "30"
#define ROUGE "31"
#define VERT  "32"
#define JAUNE "33"
#define BLEU  "34"
#define CYAN  "36"
#define BLANC "37"
#define REINIT "0"
