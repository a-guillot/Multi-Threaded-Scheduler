/*
	Andréas Guillot
*/
#include "ipc.h"

void sem_delete(int semid) {
	if (semctl(semid, 0, IPC_RMID) == -1) {
		perror("semctl");
		exit(1);
	}
}

//-----------------------------------------------------------------

int sem_create_private(int nombre, int * p) {
	int id;
	int i;

	if ((id = semget(IPC_PRIVATE, nombre, IPC_CREAT |0660)) == -1) {
		perror("semget");
		exit(1);
	}

	for (i = 0; i < nombre; i++)
	{
		if (semctl(id, i, SETVAL, p[i]) == -1) 
		{
			perror("semtcl");
			exit(1);
		}

	}

	return id;
}

//-----------------------------------------------------------------

void sem_P(int semid, int semNumber) { // puis-je ?
	struct sembuf op[1];

	op[0].sem_num = semNumber;
	op[0].sem_op = -1; // décrémente le nombre d'accès possible
	op[0].sem_flg = 0;

	semop(semid, op, 1);
}

//-----------------------------------------------------------------

void sem_V(int semid, int semNumber) { // vas-y !
	struct sembuf op[1];

	op[0].sem_num = semNumber;
	op[0].sem_op = 1;  // incrémente le nombre d'accès possible
	op[0].sem_flg = 0;

	semop(semid, op, 1);
}

//-----------------------------------------------------------------

void msg_delete(int msgid)
{
	if ((msgctl(msgid, IPC_RMID, NULL)) == -1)
	{
		fprintf(stderr, "Erreur lors de msgctl du msg déjà existant\n");
		perror("msgctl");
		exit(1);
	}
}

//-----------------------------------------------------------------

int msg_create(key_t key) {
	int id;

	if ((id = msgget(key, 0)) != -1)
	{
		// dans le cas où la file de message existe
		// déjà, on la supprime
		msg_delete(id);
	}

	if ((id = msgget(key, IPC_CREAT | 0666)) == -1)
	{
		fprintf(stderr, "Erreur lors de la création de la file de message\n");
		perror("msgget");
		exit(2);
	}

	return id;
}