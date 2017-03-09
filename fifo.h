/*
	Andréas Guillot
*/
#ifndef __FIFO_H
#define __FIFO_H
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

struct elem
{
	long valeur;
	bool isThread;	// on va enfiler deux types de message :
					// - des numéros de threads à join
					// - des travaux à donner
	struct elem * precedent;
	struct elem * suivant;
};

struct fifo
{
	struct elem * premier;
	struct elem * dernier;
	int taille;
};

struct fifo fifo_init();
void fifo_enfiler(struct fifo * f, long valeur, bool isThread);
struct elem fifo_defiler(struct fifo * f);

#endif