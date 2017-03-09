/*
	Andréas Guillot
*/
/*
	Ce fichier contient toutes les fonctions
	que j'utilise pour faciliter la manipulation
	des objets IPC. Ils sont mis dans un code à
	part afin de simplifier un fichier déjà
	relativement long.
*/

#ifndef __IPC_H
#define __IPC_H

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>


void sem_delete(int semid);
int sem_create_private(int nombre, int * p);
void sem_P(int semid, int semNumber);
void sem_V(int semid, int semNumber);

void msg_delete(int msgid);
int msg_create(key_t key);

#endif