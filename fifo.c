/*
	Andréas Guillot
*/
#include "fifo.h"

struct fifo fifo_init()
{
	struct fifo f;
	f.taille = 0;

	return f;
}

void fifo_enfiler(struct fifo * f, long valeur, bool isThread)
{
	struct elem * e;
	if ((e = malloc(sizeof(*e))) == NULL)
	{
		perror("malloc");
		exit(1);
	}
	e->valeur = valeur;
	e->isThread = isThread;

	if (f->taille == 0)
	{
		f->premier = e;
		f->dernier = e;

		e->suivant = NULL;
		e->precedent = NULL;
	}
	else
	{
		e->suivant = f->premier;
		f->premier->precedent = e;
		f->premier = e;
	}

	f->taille++;
}

struct elem fifo_defiler(struct fifo * f)
{
	struct elem res;
	if (f->taille == 0)
	{
		return res;
	}

	struct elem * last = f->dernier;
	res.valeur = last->valeur;
	res.isThread = last->isThread;

	if (f->taille == 1)
	{
		f->premier = NULL;
		f->dernier = NULL;
	}
	else
	{
		f->dernier = f->dernier->precedent;
	}

	free(last);
	f->taille--;

	return res;
}