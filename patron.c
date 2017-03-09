/*
	Andréas Guillot
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <ctype.h>

struct messageFile
{
	long mtype;     // type
	long temps;		// temps qui ira dans le nanosleep
					// long pour une structure timespec
};



int main(int argc, char * argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: ./patron temps\n");
		exit(1);
	}

	int temps = atoi(argv[1]);
	if (isdigit(temps) || temps < 0)
	{
		fprintf(stderr, "temps doit être un entier positif.\n");
		exit(2);
	}

	key_t key;
	int msgid;

	if((key = ftok("./travailleur.c", 'A')) == -1)
	{
		fprintf(stderr, "Erreur lors de la création de la clé.\n");
		perror("ftok");
		exit(3);
	}

	struct messageFile m;
	m.mtype = 1;
	m.temps = temps;

	if ((msgid = msgget(key, 0)) == -1)	// clé déjà existante
	{
		fprintf(stderr, "Erreur lors de la récupération "
						"de la file de message.\n");
		exit(4);
	}

	if ((msgsnd(msgid, &m, (sizeof(struct messageFile) - sizeof(long)), 0)) == -1)
	{
		fprintf(stderr, "Erreur lors de l'envoi d'un message.\n");
		perror("msgsnd");
		exit(5);
	}
	printf("Message %d envoyé !\n", temps);

	return 0;
}
