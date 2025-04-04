/*
 *	qsort_arg.c: qsort with a passthrough "void *" argument
 */

#include "c.h"

<<<<<<< HEAD

static char *med3(char *a, char *b, char *c,
	 qsort_arg_comparator cmp, void *arg);
static void swapfunc(char *, char *, size_t, int);

/*
 * Qsort routine based on J. L. Bentley and M. D. McIlroy,
 * "Engineering a sort function",
 * Software--Practice and Experience 23 (1993) 1249-1265.
 *
 * We have modified their original by adding a check for already-sorted input,
 * which seems to be a win per discussions on pgsql-hackers around 2006-03-21.
 *
 * Also, we recurse on the smaller partition and iterate on the larger one,
 * which ensures we cannot recurse more than log(N) levels (since the
 * partition recursed to is surely no more than half of the input).  Bentley
 * and McIlroy explicitly rejected doing this on the grounds that it's "not
 * worth the effort", but we have seen crashes in the field due to stack
 * overrun, so that judgment seems wrong.
 */

#define swapcode(TYPE, parmi, parmj, n) \
do {		\
	size_t i = (n) / sizeof (TYPE);			\
	TYPE *pi = (TYPE *)(void *)(parmi);			\
	TYPE *pj = (TYPE *)(void *)(parmj);			\
	do {						\
		TYPE	t = *pi;			\
		*pi++ = *pj;				\
		*pj++ = t;				\
		} while (--i > 0);				\
} while (0)

#define SWAPINIT(a, es) swaptype = ((char *)(a) - (char *)0) % sizeof(long) || \
	(es) % sizeof(long) ? 2 : (es) == sizeof(long)? 0 : 1

static void
swapfunc(char *a, char *b, size_t n, int swaptype)
{
	if (swaptype <= 1)
		swapcode(long, a, b, n);
	else
		swapcode(char, a, b, n);
}

#define swap(a, b)						\
	if (swaptype == 0) {					\
		long t = *(long *)(void *)(a);			\
		*(long *)(void *)(a) = *(long *)(void *)(b);	\
		*(long *)(void *)(b) = t;			\
	} else							\
		swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n) if ((n) > 0) swapfunc(a, b, n, swaptype)

static char *
med3(char *a, char *b, char *c, qsort_arg_comparator cmp, void *arg)
{
	return cmp(a, b, arg) < 0 ?
		(cmp(b, c, arg) < 0 ? b : (cmp(a, c, arg) < 0 ? c : a))
		: (cmp(b, c, arg) > 0 ? b : (cmp(a, c, arg) < 0 ? a : c));
}

void
qsort_arg(void *a, size_t n, size_t es, qsort_arg_comparator cmp, void *arg)
{
	char	   *pa,
			   *pb,
			   *pc,
			   *pd,
			   *pl,
			   *pm,
			   *pn;
	size_t		d1,
				d2;
	int			r,
				swaptype,
				presorted;

loop:SWAPINIT(a, es);
	if (n < 7)
	{
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl, arg) > 0;
				 pl -= es)
				swap(pl, pl - es);
		return;
	}
	presorted = 1;
	for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
	{
		if (cmp(pm - es, pm, arg) > 0)
		{
			presorted = 0;
			break;
		}
	}
	if (presorted)
		return;
	pm = (char *) a + (n / 2) * es;
	if (n > 7)
	{
		pl = (char *) a;
		pn = (char *) a + (n - 1) * es;
		if (n > 40)
		{
			size_t		d = (n / 8) * es;

			pl = med3(pl, pl + d, pl + 2 * d, cmp, arg);
			pm = med3(pm - d, pm, pm + d, cmp, arg);
			pn = med3(pn - 2 * d, pn - d, pn, cmp, arg);
		}
		pm = med3(pl, pm, pn, cmp, arg);
	}
	swap(a, pm);
	pa = pb = (char *) a + es;
	pc = pd = (char *) a + (n - 1) * es;
	for (;;)
	{
		while (pb <= pc && (r = cmp(pb, a, arg)) <= 0)
		{
			if (r == 0)
			{
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (r = cmp(pc, a, arg)) >= 0)
		{
			if (r == 0)
			{
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		pb += es;
		pc -= es;
	}
	pn = (char *) a + n * es;
	d1 = Min(pa - (char *) a, pb - pa);
	vecswap(a, pb - d1, d1);
	d1 = Min(pd - pc, pn - pd - es);
	vecswap(pb, pn - d1, d1);
	d1 = pb - pa;
	d2 = pd - pc;
	if (d1 <= d2)
	{
		/* Recurse on left partition, then iterate on right partition */
		if (d1 > es)
			qsort_arg(a, d1 / es, es, cmp, arg);
		if (d2 > es)
		{
			/* Iterate rather than recurse to save stack space */
			/* qsort_arg(pn - d2, d2 / es, es, cmp, arg); */
			a = pn - d2;
			n = d2 / es;
			goto loop;
		}
	}
	else
	{
		/* Recurse on right partition, then iterate on left partition */
		if (d2 > es)
			qsort_arg(pn - d2, d2 / es, es, cmp, arg);
		if (d1 > es)
		{
			/* Iterate rather than recurse to save stack space */
			/* qsort_arg(a, d1 / es, es, cmp, arg); */
			n = d1 / es;
			goto loop;
		}
	}
}
=======
#define ST_SORT qsort_arg
#define ST_ELEMENT_TYPE_VOID
#define ST_COMPARATOR_TYPE_NAME qsort_arg_comparator
#define ST_COMPARE_RUNTIME_POINTER
#define ST_COMPARE_ARG_TYPE void
#define ST_SCOPE
#define ST_DEFINE
#include "lib/sort_template.h"
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c
