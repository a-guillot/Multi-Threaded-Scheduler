/*
	Andréas Guillot
*/
#define _GNU_SOURCE // semtimedop
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "fifo.h"
#include "ipc.h"
#include <string.h>

/*
	Ces define permettent d'utiliser les sémaphores
	plus aiséments. Les deux *Lock auraient pu
	être remplacées par des mutex mais utiliser à la
	fois des mutex et des sémaphores ne semblait
	pas très pratique.
*/
#define tailleVersMain 0
#define tailleVersThreads 1
#define versThreadsLock 2
#define versMainLock 3
#define threadActif 4

struct RecruteurArgs
{
	int numeroThread;
	struct fifo * versMain;
	int * semid;
};

//-----------------------------------------------------------------

struct TravailleurArgs
{
	int numeroThread;
	int * nbThreadActif;
	struct fifo * versThreads;
	int * semid;
};

//-----------------------------------------------------------------

struct MArgs
{
	int numeroThread;
	struct fifo * versMain;
	struct fifo * versThreads;
	int * nbThreadActif;
	int * t;
	int * semid;
};

//-----------------------------------------------------------------

struct messageFile
{
	long mtype;		// type
	long temps;		// temps qui ira dans le nanosleep
					// long pour une structure timespec
};

//-----------------------------------------------------------------

void travailler(long res)
{
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = res;

	nanosleep(&t, NULL);
}

//-----------------------------------------------------------------

/*
	Un thread recruteur va attendre l'arrivée
	de travaux et les ajouter à la liste des
	travaux à effectuer.
*/
void * threadRecruteur(struct RecruteurArgs * args)
{
	/*
		On pourrait créer la file de message dans le thread principal,
		mais vu que ce thread est unique est qu'elle est seulement
		utilisée ici on peut la faire ici
	*/
	key_t key;
	if ((key = ftok("./travailleur.c", 'A')) == -1)
	{
		fprintf(stderr, "Echec de la création d'une clé\n");
		perror("ftok");
		exit(1);
	}
	int msgid = msg_create(key);	// cette fonction gère aussi
									// si la msg existe déjà
	bool finished = false;			// savoir si on continue à boucler
	bool isThread;					// savoir si thread ou travail

	printf("Thread recruteur : Prêt à travailler \n");

	while (!finished)
	{
		/*
		On reçoit le premier message de la file et on
		le mets à l'adresse du long value.
		Bloquant jusqu'à la réception d'un message.
		*/
		struct messageFile test;
		if ((msgrcv(msgid, &test, sizeof(long), 0, 0)) == -1)
		{
			fprintf(stderr, "Erreur lors de la réception d'un message.\n");
			perror("msgrcv");
			exit(1);
		}
		long value = test.temps;
		isThread = false;

		printf("Thread recruteur : j'ai reçu un message valeur %li.\n", value);
		/* Action différente selon le contenu de value*/
		if (value == 0)
		{
			/*
				Cas de fin du programme. Ce thread va free et s'ajouter
				à la fifo qui va vers le thread principal, ce qui signifie
				qu'il doit se terminer.
			*/
			finished = true;
		}

		/*
			On lock/unlock l'accès à la file.
			On incrémente aussi le sémaphore 
			correspondant à la taille de la fifo
		*/
		printf("Thread recruteur : je me prépare à enfiler.\n");
		sem_P(*(args->semid), versMainLock);

		fifo_enfiler(args->versMain, value, isThread);
		printf("Thread recruteur : j'ai enfilé quelque chose...\n");
		sem_V(*(args->semid), tailleVersMain);

		sem_V(*(args->semid), versMainLock);
	}

	/*
		On peut créer/supprimer la file de messages
		ici car c'est le seul à l'utiliser.
		On aurait aussi pu le créer dans le main
	*/
	msg_delete(msgid);
	printf("Thread recruteur : je me termine.\n");

	return (NULL);
}

//-----------------------------------------------------------------

/*
	Un thread travailleur est un des threads
	toujours actif qui va attendre un travail
*/
void * threadTravailleur(struct TravailleurArgs * args)
{
	bool finished = false;
	struct elem res;
	printf("Thread travailleur %d : prêt à travailler.\n", 
			args->numeroThread);

	while (!finished)
	{
		// on bloque l'accès à la fifo
		sem_P(*(args->semid), tailleVersThreads);
		// printf("pas normal\n");
		sem_P(*(args->semid), versThreadsLock);

		// on récupère ce qu'il faut
		res = fifo_defiler(args->versThreads);

		// on rend l'accès à la fifo
		sem_V(*(args->semid), versThreadsLock);

		printf("Thread travailleur %d se met au travail.\n",
				args->numeroThread);

		if (res.valeur == 0)
		{
			finished = true;
			printf("Thread travailleur %d : j'ai reçu un 0.\n", 
					args->numeroThread);
		}
		else
		{
			// on augmente le nombre de threads actifs
			sem_P(*(args->semid), threadActif);
			*(args->nbThreadActif) = *(args->nbThreadActif) + 1;
			sem_V(*(args->semid), threadActif);

			// on travaille
			travailler(res.valeur);

			printf("Thread travailleur %d a fini de travailler.\n",
					args->numeroThread);

			// on diminue le nombre de threads actifs
			sem_P(*(args->semid), threadActif);
			*(args->nbThreadActif) = *(args->nbThreadActif) - 1;
			sem_V(*(args->semid), threadActif);

			printf("Thread travailleur %d se remet en attente.\n", 
					args->numeroThread);
		}
	}
	return NULL;
}

//-----------------------------------------------------------------

/*
	Un thread M va être créé quand il
	y a trop de travail pour les threads travailleurs.
	Maximum de m - n threads M.
	Se trouve dans le même tableau que les N
*/
void * threadM(struct MArgs * args)
{
	bool finished = false;
	struct timespec ts;
	struct sembuf sops[1];
	int res;

	printf("Thread M %d : un thread aditionnel a été créé.\n",
			args->numeroThread);
	while (!finished)
	{
		/*
			On va utiliser semtimedop pour attendre que la valeur
			du sémaphore passe à 1

			int semtimedop(int semid, struct sembuf *sops, unsigned nsops,
                      struct timespec *timeout);
		*/
		// timeout
		ts.tv_sec = *(args->t);
		ts.tv_nsec = 0;

		// struct sembuf
		sops[0].sem_num = tailleVersThreads;
		sops[0].sem_op = -1;				// l'action que l'on
		sops[0].sem_flg = 0;				// souhaite faire

		printf("Thread M %d : Se prépare à rentrer en attente.\n",
			args->numeroThread);

		res = semtimedop(*(args->semid), sops, 1, &ts);
		if (res == 0)
		{

			// est rentré avant la fin du timeout
			printf("Thread M %d : A reçu un travail.\n",
					args->numeroThread);

			// on bloque l'accès à la fifo
			sem_P(*(args->semid), versThreadsLock);

			// on récupère ce qu'il faut
			struct elem e = fifo_defiler(args->versThreads);

			// on rend l'accès à la fifo
			sem_V(*(args->semid), versThreadsLock);

			if (e.valeur == 0)
			{
				finished = true;
				printf("Thread M %d : j'ai reçu un 0.", args->numeroThread);
			}
			else
			{
				printf("Thread M %d se met au travail.\n",
						args->numeroThread);

				// on augmente le nombre de threads actifs
				sem_P(*(args->semid), threadActif);
				*(args->nbThreadActif) = *(args->nbThreadActif) + 1;
				sem_V(*(args->semid), threadActif);

				// on travaille
				travailler(e.valeur);

				printf("Thread M %d a fini de travailler.\n",
						args->numeroThread);

				// on diminue le nombre de threads actifs
				sem_P(*(args->semid), threadActif);
				*(args->nbThreadActif) = *(args->nbThreadActif) - 1;
				sem_V(*(args->semid), threadActif);
			}
		}
		else
		{
			// timeout : s'ajoute à la fifo et se termine
			printf("Thread M %d : N'a pas reçu de travail.\n",
					args->numeroThread);

			sem_P(*(args->semid), versMainLock);

			// il enfile son propre numéro
			fifo_enfiler(args->versMain, args->numeroThread, 1); 

			printf("Thread M %d : S'est enfilé dans la file.\n",
					args->numeroThread);

			sem_V(*(args->semid), tailleVersMain); // augmente la taille

			sem_V(*(args->semid), versMainLock);   // rend la main

			finished = true;
		}

	}
	printf("Thread M %d : Se termine.\n",
					args->numeroThread);
	return NULL;
}

//-----------------------------------------------------------------

void testArguments(int n, int m, int t)
{
	if (isdigit(n) || isdigit(m) || isdigit(t))
	{
		fprintf(stderr, "Les paramètres doivent être des entiers.\n");
		exit(1);
	}
	if (n <= 0)
	{
		fprintf(stderr, "n doit être strictement supérieur à 0.\n");
		exit(2);
	}
	if (m < 0)
	{
		fprintf(stderr, "m doit être supérieur ou égal à 0.\n");
		exit(3);
	}
	if (t < 0)
	{
		fprintf(stderr, "t doit être supérieur ou égal à 0.\n");
		exit(4);
	}
	if (n >= m)
	{
		fprintf(stderr, "m doit être supérieur à n.\n");
		exit(5);
	}
}

//-----------------------------------------------------------------

int main(int argc, char * argv[])
{
	if (argc != 4)
	{
		fprintf(stderr, "Usage: travailleur.c n m t\n");
		exit(1);
	}

	/* Variables */

	// Valeurs reçues en paramètre
	int n = atoi(argv[1]);	// nombre de threads présents à tous moments
	int m = atoi(argv[2]);	// nombre de threads maximal
	int t = atoi(argv[3]);	// temps d'attente des threads 
							// >m une fois le travail terminé

	testArguments(n, m, t);
	
	// Valeurs utilitaires
	int i;
	int nbThreadActif = 0;
	int nombreThreads = n;	// contient n + le nombre de m actifs
	bool finished = false;	// la boucle du main

	// Threads
	pthread_t recruteur;
	pthread_t travailleurs[n];
	pthread_t mthreads[m - n];

	// initialisation
	for (i = 0; i < (m - n); i++)
		mthreads[i] = -1;

	// Arguments de threads
	struct RecruteurArgs recruteurArgs;
	struct TravailleurArgs nArgs[n];
	struct MArgs mArgs;

	// Sémaphores
	int semValues[5] = { 0, 0, 1, 1, 1 };
	int semid = sem_create_private(5, semValues);

	// file contenant les travaux
	struct fifo versThreads;

	// file contenant les threads étant terminés
	struct fifo versMain;

	/* Création des deux fifo */
	versThreads = fifo_init();
	versMain = fifo_init();

	/* Initialisation du thread recruteur de travaux */
	recruteurArgs.numeroThread = 0;
	recruteurArgs.versMain = &versMain;
	recruteurArgs.semid = &semid;

	if ((pthread_create(&recruteur,
						NULL, 
						(void *)&threadRecruteur, 
						&recruteurArgs)) == -1)
	{
		fprintf(stderr, "Erreur lors de la création du thread recruteur.\n");
		perror("pthread_create");
		exit(3);
	}

	for (i = 0; i < n; i++)
	{
		nArgs[i].numeroThread = i;
		nArgs[i].nbThreadActif = &nbThreadActif;
		nArgs[i].versThreads = &versThreads;
		nArgs[i].semid = &semid;

		printf("Thread principal : création du thread N numéro %d\n", i);

		if ((pthread_create(&travailleurs[i], 
							NULL, 
							(void *)&threadTravailleur, 
							&nArgs[i])) == -1)
		{
			fprintf(stderr, "Erreur lors de la création des Nième");
			perror("pthread_create");
			exit(4);
		}
	}

	// attente passive sur la valeur de fifo versMain
	while (!finished)
	{
		// on diminue le sem sur la taille de la fifo
		sem_P(semid, tailleVersMain);		
		printf("Thread principal : j'ai reçu quelque chose...\n");

		// bloquant jusqu'à ce que versMain contienne quelque chose
		sem_P(semid, versMainLock);

		struct elem e = fifo_defiler(&versMain);
		bool isThread = e.isThread;
		long valeur = e.valeur;

		sem_V(semid, versMainLock);

		if (isThread)
		{
			printf("Thread principal : la valeur que j'ai "
					"reçue est un thread\n");
			// on doit join le thread de numéro valeur
			if ((pthread_join(mthreads[valeur], NULL)) != 0)
			{
				fprintf(stderr, "Erreur lors de l'arrêt d'un thread.\n");
				perror("pthread_join");
				exit(5);
			}
			mthreads[valeur] = -1;	// on le réinitialise à -1
			nombreThreads--;		// on diminue le nb de thread courants.
		}
		else
		{
			printf("Thread principal : la valeur que j'ai "
					"reçue n'est pas un thread\n");
			// on envoie le travail aux autres (duree = valeur)

			if (valeur == 0)
			{
				// on doit tout arrêter
				finished = true;	// sortira de la boucle et terminera tout
				printf("Thread principal : j'ai reçu 0, "
						"je sors de mon attente.\n");

				/*
					On enfile autant de 0 qu'il y a de threads.
				*/
				for (i = 0; i < nombreThreads; i++)
				{
					sem_P(semid, versThreadsLock);
					fifo_enfiler(&versThreads, 0, false);
					sem_V(semid, versThreadsLock);

					sem_V(semid, tailleVersThreads);	// tout en débloquant
				}										// les threads
				printf("Thread principal : j'ai enfilé %d 0.\n", 
						nombreThreads);

			}
			else
			{
				// deux cas de figure :
				// - le cas standard des threads travailleurs
				// - celui où tous les threads travailleurs
				//   sont actifs et qu'il faut créer des
				//   threads M.
				bool overbooked = false;
				
				sem_P(semid, threadActif);
				if (nbThreadActif >= n)
					overbooked = true;				
				sem_V(semid, threadActif);

				if (overbooked)	// on crée des M pour compenser
				{
					for (i = 0; i < (m - n); i++)
					{
						if (mthreads[i] == (unsigned long)-1)
						{
							mArgs.numeroThread = i;
							mArgs.versMain = &versMain;
							mArgs.versThreads = &versThreads;
							mArgs.nbThreadActif = &nbThreadActif;
							mArgs.t = &t;
							mArgs.semid = &semid;

							if ((pthread_create(&mthreads[i], 
								NULL, 
								(void *)&threadM, 
								&mArgs)) == -1)
							{
								fprintf(stderr, "Erreur lors de la "
												"création des Mième");
								perror("pthread_create");
								exit(4);
							}
							printf("Thread principal : Je viens de "
								"créer un thread M pour aider.\n");
							nombreThreads++;
						}
					}
					// si jamais tous les threads sont actifs,
					// alors on ne fait rien.
				}
				// cas standard vers threads travailleurs
				sem_P(semid, versThreadsLock);

				fifo_enfiler(&versThreads, valeur, false);
				printf("Thread principal : j'ai enfilé quelque "
					"chose de valeur %li\n", valeur);

				sem_V(semid, versThreadsLock);

				// on augmente la taille de la fifo versThreads	
				sem_V(semid, tailleVersThreads);
			}
		}
	}

	printf("Thread principal : j'attends la "
			"terminaison de tous les threads.\n");

	if ((pthread_join(recruteur, NULL)) != 0)
	{
		fprintf(stderr, "Erreur lors du pthread_join "
				"du thread recruteur\n");
		perror("pthread_join");
		exit(5);
	}

	for (i = 0; i < n; i++)
	{
		if ((pthread_join(travailleurs[i], NULL)) != 0)
		{
			fprintf(stderr, "Erreur lors du pthread_join "
					"d'un des threads travailleurs\n");
			perror("pthread_join");
			exit(5);
		}
	}

	printf("Threads terminés ! \n");

	/* suppression msg+free */
	sem_delete(semid); // avant car mes join ne marchent pas encore


	printf("\nFin du programme ! \n");
	return 0;
}