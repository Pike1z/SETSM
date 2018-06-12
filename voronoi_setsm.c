/* This software was adapted from the voronoi algorithm by Steven Fortune 
(http://ect.bell-labs.com/who/sjf/) as modified by Derek Bradley
(http://zurich.disneyresearch.com/derekbradley/voronoi.html)

Reference: Steve J. Fortune (1987) A Sweepline Algorithm for Voronoi Diagrams,
Algorithmica 2, 153-174. */

/*** VORONOI.C ***/

#include "voronoi_setsm.h"
#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//edgelist
int ELhashsize ;
Site * bottomsite ;
Freelist hfl ;
Halfedge * ELleftend, * ELrightend, **ELhash ;
int ntry, totalsearch ;

//geometry
double deltax, deltay ;
int nsites, nedges, sqrt_nsites, nvertices ;
Freelist efl ;

//heap
int PQmin, PQcount, PQhashsize ;
Halfedge * PQhash ;

//memory
int siteidx ;
char** memory_map;
int nallocs = 0;

//output
int plot, debug;
double ymax, ymin, xmax, xmin ;
FILE *fid_bisector, *fid_ep, *fid_vertex, *fid_site, *fid_triple;
double pxmin, pxmax, pymin, pymax, cradius;

//main.c
int triangulate, sorted, siteidx;
Site * sites;
Freelist sfl;

void initializeVoronoi(void)
{

	sorted = plot = debug = 0;
	triangulate = 1;
	freeinit(&sfl, sizeof(Site));

	siteidx = 0;
}

int scomp(const void * vs1, const void * vs2)
{
	Point * s1 = (Point *)vs1 ;
	Point * s2 = (Point *)vs2 ;

	if (s1->y < s2->y)
	{
		return (-1) ;
	}
	if (s1->y > s2->y)
	{
		return (1) ;
	}
	if (s1->x < s2->x)
	{
		return (-1) ;
	}
	if (s1->x > s2->x)
	{
		return (1) ;
	}
	return (0) ;
}

/*** return a single in-storage site ***/

Site *nextone(void)
{
	Site * s ;

	if (siteidx < nsites)
	{
		s = &sites[siteidx++];
		return (s) ;
	}
	else
	{
		return ((Site *)NULL) ;
	}
}

/*** read all sites, sort, and compute xmin, xmax, ymin, ymax ***/

void readsites(D3DPOINT *ptslists,int numofpts)
{
	int i ;
	nsites = 0 ;
	sites = (Site *) myalloc(numofpts * sizeof(Site));
	i = 0;
	for(i=0;i<numofpts;i++)
	{
		sites[nsites].coord.x = ptslists[i].m_X;
		sites[nsites].coord.y = ptslists[i].m_Y;
		sites[nsites].sitenbr = nsites ;
		sites[nsites++].refcnt = 0 ;
	}

	qsort((void *)sites, nsites, sizeof(Site), scomp) ;
	if (nsites > 0)
	{
		xmin = sites[0].coord.x ;
		xmax = sites[0].coord.x ;
		for (i = 1 ; i < nsites ; ++i)
		{
			if(sites[i].coord.x < xmin)
			{
				xmin = sites[i].coord.x ;
			}
			if (sites[i].coord.x > xmax)
			{
				xmax = sites[i].coord.x ;
			}
		}
		ymin = sites[0].coord.y ;
		ymax = sites[nsites-1].coord.y ;
	}
}

/*** read one site ***/

Site *readone(void)
{
	Site * s ;

	s = (Site *)getfree(&sfl) ;
	s->refcnt = 0 ;
	s->sitenbr = siteidx++ ;
	if (scanf("%f %f", &(s->coord.x), &(s->coord.y)) == EOF)
	{
		return ((Site *)NULL ) ;
	}
	return (s) ;
}

//edgelist.c

void
ELinitialize(void)
{
	int i ;
	
	freeinit(&hfl, sizeof(Halfedge)) ;
	ELhashsize = 2 * sqrt_nsites ;
	ELhash = (Halfedge **)myalloc( sizeof(*ELhash) * ELhashsize) ;
	for (i = 0	; i < ELhashsize  ; i++)
	{
		ELhash[i] = (Halfedge *)NULL ;
	}
	ELleftend = HEcreate((Edge *)NULL, 0) ;
	ELrightend = HEcreate((Edge *)NULL, 0) ;
	ELleftend->ELleft = (Halfedge *)NULL ;
	ELleftend->ELright = ELrightend ;
	ELrightend->ELleft = ELleftend ;
	ELrightend->ELright = (Halfedge *)NULL ;
	ELhash[0] = ELleftend ;
	ELhash[ELhashsize-1] = ELrightend ;
}

Halfedge *
HEcreate(Edge * e, int pm)
{
	Halfedge * answer ;
	
	answer = (Halfedge *)getfree(&hfl) ;
	answer->ELedge = e ;
	answer->ELpm = pm ;
	answer->PQnext = (Halfedge *)NULL ;
	answer->vertex = (Site *)NULL ;
	answer->ELrefcnt = 0 ;
	return (answer) ;
}

void
ELinsert(Halfedge * lb, Halfedge * new)
{
	new->ELleft = lb ;
	new->ELright = lb->ELright ;
	(lb->ELright)->ELleft = new ;
	lb->ELright = new ;
}

/* Get entry from hash table, pruning any deleted nodes */

Halfedge *
ELgethash(int b)
{
	Halfedge * he ;
	
	if ((b < 0) || (b >= ELhashsize))
	{
		return ((Halfedge *)NULL) ;
	}
	he = ELhash[b] ;
	if ((he == (Halfedge *)NULL) || (he->ELedge != (Edge *)DELETED))
	{
		return (he) ;
	}
	/* Hash table points to deleted half edge.	Patch as necessary. */
	ELhash[b] = (Halfedge *)NULL ;
	if ((--(he->ELrefcnt)) == 0)
	{
		makefree((Freenode *)he, (Freelist *)&hfl) ;
	}
	return ((Halfedge *)NULL) ;
}

Halfedge *
ELleftbnd(Point * p)
{
	int i, bucket ;
	Halfedge * he ;
	
	/* Use hash table to get close to desired halfedge */
	bucket = (p->x - xmin) / deltax * ELhashsize ;
	if (bucket < 0)
	{
		bucket = 0 ;
	}
	if (bucket >= ELhashsize)
	{
		bucket = ELhashsize - 1 ;
	}
	he = ELgethash(bucket) ;
	if	(he == (Halfedge *)NULL)
	{
		for (i = 1 ; 1 ; i++)
		{
			if ((he = ELgethash(bucket-i)) != (Halfedge *)NULL)
			{
				break ;
			}
			if ((he = ELgethash(bucket+i)) != (Halfedge *)NULL)
			{
				break ;
			}
		}
		totalsearch += i ;
	}
	ntry++ ;
	/* Now search linear list of halfedges for the corect one */
	if (he == ELleftend || (he != ELrightend && right_of(he,p)))
	{
		do	{
			he = he->ELright ;
		} while (he != ELrightend && right_of(he,p)) ;
		he = he->ELleft ;
	}
	else
	{
		do	{
			he = he->ELleft ;
		} while (he != ELleftend && !right_of(he,p)) ;
	}
	/*** Update hash table and reference counts ***/
	if ((bucket > 0) && (bucket < ELhashsize-1))
	{
		if (ELhash[bucket] != (Halfedge *)NULL)
		{
			(ELhash[bucket]->ELrefcnt)-- ;
		}
		ELhash[bucket] = he ;
		(ELhash[bucket]->ELrefcnt)++ ;
	}
	return (he) ;
}

/*** This delete routine can't reclaim node, since pointers from hash
	 : table may be present.
***/

void
ELdelete(Halfedge * he)
{
	(he->ELleft)->ELright = he->ELright ;
	(he->ELright)->ELleft = he->ELleft ;
	he->ELedge = (Edge *)DELETED ;
}

Halfedge *
ELright(Halfedge * he)
{
	return (he->ELright) ;
}

Halfedge *
ELleft(Halfedge * he)
{
	return (he->ELleft) ;
}

Site *
leftreg(Halfedge * he)
{
	if (he->ELedge == (Edge *)NULL)
	{
		return(bottomsite) ;
	}
	return (he->ELpm == le ? he->ELedge->reg[le] :
			he->ELedge->reg[re]) ;
}

Site *
rightreg(Halfedge * he)
{
	if (he->ELedge == (Edge *)NULL)
	{
		return(bottomsite) ;
	}
	return (he->ELpm == le ? he->ELedge->reg[re] :
			he->ELedge->reg[le]) ;
}

/*** GEOMETRY.C ***/

void
geominit(void)
{
	freeinit(&efl, sizeof(Edge)) ;
	nvertices = nedges = 0 ;
	sqrt_nsites = sqrt(nsites+4) ;
	deltay = ymax - ymin ;
	deltax = xmax - xmin ;
}

Edge *
bisect(Site * s1, Site * s2)
{
	double dx, dy, adx, ady ;
	Edge * newedge ;
	
	newedge = (Edge *)getfree(&efl) ;
	newedge->reg[0] = s1 ;
	newedge->reg[1] = s2 ;
	ref(s1) ;
	ref(s2) ;
	newedge->ep[0] = newedge->ep[1] = (Site *)NULL ;
	dx = s2->coord.x - s1->coord.x ;
	dy = s2->coord.y - s1->coord.y ;
	adx = dx>0 ? dx : -dx ;
	ady = dy>0 ? dy : -dy ;
	newedge->c = s1->coord.x * dx + s1->coord.y * dy + (dx*dx +
														dy*dy) * 0.5 ;
	if (adx > ady)
	{
		newedge->a = 1.0 ;
		newedge->b = dy/dx ;
		newedge->c /= dx ;
	}
	else
	{
		newedge->b = 1.0 ;
		newedge->a = dx/dy ;
		newedge->c /= dy ;
	}
	newedge->edgenbr = nedges ;
	out_bisector(newedge) ;
	nedges++ ;
	return (newedge) ;
}

Site *
intersect(Halfedge * el1, Halfedge * el2)
{
	Edge * e1, * e2, * e ;
	Halfedge * el ;
	double d, xint, yint ;
	int right_of_site ;
	Site * v ;
	
	e1 = el1->ELedge ;
	e2 = el2->ELedge ;
	if ((e1 == (Edge*)NULL) || (e2 == (Edge*)NULL))
	{
		return ((Site *)NULL) ;
	}
	if (e1->reg[1] == e2->reg[1])
	{
		return ((Site *)NULL) ;
	}
	d = (e1->a * e2->b) - (e1->b * e2->a) ;
	if ((-1.0e-10 < d) && (d < 1.0e-10))
	{
		return ((Site *)NULL) ;
	}
	xint = (e1->c * e2->b - e2->c * e1->b) / d ;
	yint = (e2->c * e1->a - e1->c * e2->a) / d ;
	if ((e1->reg[1]->coord.y < e2->reg[1]->coord.y) ||
		(e1->reg[1]->coord.y == e2->reg[1]->coord.y &&
		 e1->reg[1]->coord.x < e2->reg[1]->coord.x))
	{
		el = el1 ;
		e = e1 ;
	}
	else
	{
		el = el2 ;
		e = e2 ;
	}
	right_of_site = (xint >= e->reg[1]->coord.x) ;
	if ((right_of_site && (el->ELpm == le)) ||
		(!right_of_site && (el->ELpm == re)))
	{
		return ((Site *)NULL) ;
	}
	v = (Site *)getfree(&sfl) ;
	v->refcnt = 0 ;
	v->coord.x = xint ;
	v->coord.y = yint ;
	return (v) ;
}

/*** returns 1 if p is to right of halfedge e ***/

int
right_of(Halfedge * el, Point * p)
{
	Edge * e ;
	Site * topsite ;
	int right_of_site, above, fast ;
	double dxp, dyp, dxs, t1, t2, t3, yl ;
	
	e = el->ELedge ;
	topsite = e->reg[1] ;
	right_of_site = (p->x > topsite->coord.x) ;
	if (right_of_site && (el->ELpm == le))
	{
		return (1) ;
	}
	if(!right_of_site && (el->ELpm == re))
	{
		return (0) ;
	}
	if (e->a == 1.0)
	{
		dyp = p->y - topsite->coord.y ;
		dxp = p->x - topsite->coord.x ;
		fast = 0 ;
		if ((!right_of_site & (e->b < 0.0)) ||
			(right_of_site & (e->b >= 0.0)))
		{
			fast = above = (dyp >= e->b*dxp) ;
		}
		else
		{
			above = ((p->x + p->y * e->b) > (e->c)) ;
			if (e->b < 0.0)
			{
				above = !above ;
			}
			if (!above)
			{
				fast = 1 ;
			}
		}
		if (!fast)
		{
			dxs = topsite->coord.x - (e->reg[0])->coord.x ;
			above = (e->b * (dxp*dxp - dyp*dyp))
				<
				(dxs * dyp * (1.0 + 2.0 * dxp /
							  dxs + e->b * e->b)) ;
			if (e->b < 0.0)
			{
				above = !above ;
			}
		}
	}
	else  /*** e->b == 1.0 ***/
	{
		yl = e->c - e->a * p->x ;
		t1 = p->y - yl ;
		t2 = p->x - topsite->coord.x ;
		t3 = yl - topsite->coord.y ;
		above = ((t1*t1) > ((t2 * t2) + (t3 * t3))) ;
	}
	return (el->ELpm == le ? above : !above) ;
}

void
endpoint(Edge * e, int lr, Site * s)
{
	e->ep[lr] = s ;
	ref(s) ;
	if (e->ep[re-lr] == (Site *)NULL)
	{
		return ;
	}
	out_ep(e) ;
	deref(e->reg[le]) ;
	deref(e->reg[re]) ;
	makefree((Freenode *)e, (Freelist *) &efl) ;
}

double
dist(Site * s, Site * t)
{
	double dx,dy ;
	
	dx = s->coord.x - t->coord.x ;
	dy = s->coord.y - t->coord.y ;
	return (sqrt(dx*dx + dy*dy)) ;
}

void
makevertex(Site * v)
{
	v->sitenbr = nvertices++ ;
	out_vertex(v) ;
}

void
deref(Site * v)
{
	if (--(v->refcnt) == 0 )
	{
		makefree((Freenode *)v, (Freelist *)&sfl) ;
	}
}

void
ref(Site * v)
{
	++(v->refcnt) ;
}

/*** HEAP.C ***/

void
PQinsert(Halfedge * he, Site * v, double offset)
{
	Halfedge * last, * next ;
	
	he->vertex = v ;
	ref(v) ;
	he->ystar = v->coord.y + offset ;
	last = &PQhash[ PQbucket(he)] ;
	while ((next = last->PQnext) != (Halfedge *)NULL &&
		   (he->ystar  > next->ystar  ||
			(he->ystar == next->ystar &&
			 v->coord.x > next->vertex->coord.x)))
	{
		last = next ;
	}
	he->PQnext = last->PQnext ;
	last->PQnext = he ;
	PQcount++ ;
}

void
PQdelete(Halfedge * he)
{
	Halfedge * last;
	
	if(he ->  vertex != (Site *) NULL)
	{
		last = &PQhash[PQbucket(he)] ;
		while (last -> PQnext != he)
		{
			last = last->PQnext ;
		}
		last->PQnext = he->PQnext;
		PQcount-- ;
		deref(he->vertex) ;
		he->vertex = (Site *)NULL ;
	}
}

int
PQbucket(Halfedge * he)
{
	int bucket ;
	
	
	if		(he->ystar < ymin)	bucket = 0;
	else if (he->ystar >= ymax) bucket = PQhashsize-1;
	else			bucket = (he->ystar - ymin)/deltay * PQhashsize;
	if (bucket < 0)
	{
		bucket = 0 ;
	}
	if (bucket >= PQhashsize)
	{
		bucket = PQhashsize-1 ;
	}
	if (bucket < PQmin)
	{
		PQmin = bucket ;
	}
	return (bucket);
}

int
PQempty(void)
{
	return (PQcount == 0) ;
}


Point
PQ_min(void)
{
	Point answer ;
	
	while (PQhash[PQmin].PQnext == (Halfedge *)NULL)
	{
		++PQmin ;
	}
	answer.x = PQhash[PQmin].PQnext->vertex->coord.x ;
	answer.y = PQhash[PQmin].PQnext->ystar ;
	return (answer) ;
}

Halfedge *
PQextractmin(void)
{
	Halfedge * curr ;
	
	curr = PQhash[PQmin].PQnext ;
	PQhash[PQmin].PQnext = curr->PQnext ;
	PQcount-- ;
	return (curr) ;
}

void
PQinitialize(void)
{
	int i ;
	
	PQcount = PQmin = 0 ;
	PQhashsize = 4 * sqrt_nsites ;
	PQhash = (Halfedge *)myalloc(PQhashsize * sizeof *PQhash) ;
	for (i = 0 ; i < PQhashsize; i++)
	{
		PQhash[i].PQnext = (Halfedge *)NULL ;
	}
}



/*** MEMORY.C ***/

void
freeinit(Freelist * fl, int size)
{
	fl->head = (Freenode *)NULL ;
	fl->nodesize = size ;
}

char *
getfree(Freelist * fl)
{
	int i ;
	Freenode * t ;
	if (fl->head == (Freenode *)NULL)
	{
		t =	 (Freenode *) myalloc(sqrt_nsites * fl->nodesize) ;
		for(i = 0 ; i < sqrt_nsites ; i++)
		{
			makefree((Freenode *)((char *)t+i*fl->nodesize), fl) ;
		}
	}
	t = fl->head ;
	fl->head = (fl->head)->nextfree ;
	return ((char *)t) ;
}

void
makefree(Freenode * curr, Freelist * fl)
{
	curr->nextfree = fl->head ;
	fl->head = curr ;
}

int total_alloc;

char *
myalloc(unsigned n)
{
	char * t ;
	if ((t=(char*)malloc(n)) == (char *) 0)
	{
		fprintf(stderr,"Insufficient memory processing site %d (%d bytes in use)\n",
				siteidx, total_alloc) ;
		exit(0) ;
	}
	total_alloc += n ;
	
	if (nallocs % 1000 == 0)
	{
		if (nallocs == 0)
			memory_map = (char **)malloc((nallocs+1000)*sizeof(char*));
		else
			memory_map = (char **)realloc(memory_map,(nallocs+1000)*sizeof(char*));
	}
	memory_map[nallocs++] = t;
	return (t) ;
}

void free_all(void)
{
	int i;
	
	for (i=0; i<nallocs; i++)
	{
		if (memory_map[i] != (char*)0)
		{
			free(memory_map[i]);
			memory_map[i] = (char*)0;
		}
	}
	
	free(memory_map);
	nallocs = 0;
}




/*** OUTPUT.C ***/

void
openpl(void)
{
}

void
line(double ax, double ay, double bx, double by)
{
}

void
circle(double ax, double ay, double radius)
{
}

void
range(double pxmin, double pxmax, double pymin, double pymax)
{
}

void
out_bisector(Edge * e)
{
	if (triangulate && plot && !debug)
	{
		line(e->reg[0]->coord.x, e->reg[0]->coord.y,
			 e->reg[1]->coord.x, e->reg[1]->coord.y) ;
	}
	if (!triangulate && !plot && !debug)
	{
		printf("l %f %f %f\n", e->a, e->b, e->c) ;
	}
	if (debug)
	{
		printf("line(%d) %gx+%gy=%g, bisecting %d %d\n", e->edgenbr,
			   e->a, e->b, e->c, e->reg[le]->sitenbr, e->reg[re]->sitenbr) ;
	}
}

void
out_ep(Edge * e)
{
	if (!triangulate && plot)
	{
		clip_line(e) ;
	}
	if (!triangulate && !plot)
	{
		printf("e %d", e->edgenbr);
		printf(" %d ", e->ep[le] != (Site *)NULL ? e->ep[le]->sitenbr : -1) ;
		printf("%d\n", e->ep[re] != (Site *)NULL ? e->ep[re]->sitenbr : -1) ;
	}
}

void
out_vertex(Site * v)
{
	if (!triangulate && !plot && !debug)
	{
		printf ("v %f %f\n", v->coord.x, v->coord.y) ;
	}
	if (debug)
	{
		printf("vertex(%d) at %f %f\n", v->sitenbr, v->coord.x, v->coord.y) ;
	}
}

void
out_site(Site * s)
{
	if (!triangulate && plot && !debug)
	{
		circle (s->coord.x, s->coord.y, cradius) ;
	}
	if (!triangulate && !plot && !debug)
	{
		printf("s %f %f\n", s->coord.x, s->coord.y) ;
	}
	if (debug)
	{
		printf("site (%d) at %f %f\n", s->sitenbr, s->coord.x, s->coord.y) ;
	}
}

void
out_triple(Site * s1, Site * s2, Site * s3, int *count_tri)
{
	if (triangulate && !plot && !debug)
	{
		(*count_tri)++;
		fprintf(fid_triple,"%d %d %d\n", s1->sitenbr, s2->sitenbr, s3->sitenbr) ;
		//printf("%d %d %d\n", s1->sitenbr, s2->sitenbr, s3->sitenbr) ;
	}
	if (debug)
	{
		printf("circle through left=%d right=%d bottom=%d\n",
			   s1->sitenbr, s2->sitenbr, s3->sitenbr) ;
	}
}

void
plotinit(void)
{
	double dx, dy, d ;
	
	dy = ymax - ymin ;
	dx = xmax - xmin ;
	d = ( dx > dy ? dx : dy) * 1.1 ;
	pxmin = xmin - (d-dx) / 2.0 ;
	pxmax = xmax + (d-dx) / 2.0 ;
	pymin = ymin - (d-dy) / 2.0 ;
	pymax = ymax + (d-dy) / 2.0 ;
	cradius = (pxmax - pxmin) / 350.0 ;
	openpl() ;
	range(pxmin, pymin, pxmax, pymax) ;
}

void
clip_line(Edge * e)
{
	Site * s1, * s2 ;
	double x1, x2, y1, y2 ;
	
	if (e->a == 1.0 && e->b >= 0.0)
	{
		s1 = e->ep[1] ;
		s2 = e->ep[0] ;
	}
	else
	{
		s1 = e->ep[0] ;
		s2 = e->ep[1] ;
	}
	if (e->a == 1.0)
	{
		y1 = pymin ;
		if (s1 != (Site *)NULL && s1->coord.y > pymin)
		{
			y1 = s1->coord.y ;
		}
		if (y1 > pymax)
		{
			return ;
		}
		x1 = e->c - e->b * y1 ;
		y2 = pymax ;
		if (s2 != (Site *)NULL && s2->coord.y < pymax)
		{
			y2 = s2->coord.y ;
		}
		if (y2 < pymin)
		{
			return ;
		}
		x2 = e->c - e->b * y2 ;
		if (((x1 > pxmax) && (x2 > pxmax)) || ((x1 < pxmin) && (x2 < pxmin)))
		{
			return ;
		}
		if (x1 > pxmax)
		{
			x1 = pxmax ;
			y1 = (e->c - x1) / e->b ;
		}
		if (x1 < pxmin)
		{
			x1 = pxmin ;
			y1 = (e->c - x1) / e->b ;
		}
		if (x2 > pxmax)
		{
			x2 = pxmax ;
			y2 = (e->c - x2) / e->b ;
		}
		if (x2 < pxmin)
		{
			x2 = pxmin ;
			y2 = (e->c - x2) / e->b ;
		}
	}
	else
	{
		x1 = pxmin ;
		if (s1 != (Site *)NULL && s1->coord.x > pxmin)
		{
			x1 = s1->coord.x ;
		}
		if (x1 > pxmax)
		{
			return ;
		}
		y1 = e->c - e->a * x1 ;
		x2 = pxmax ;
		if (s2 != (Site *)NULL && s2->coord.x < pxmax)
		{
			x2 = s2->coord.x ;
		}
		if (x2 < pxmin)
		{
			return ;
		}
		y2 = e->c - e->a * x2 ;
		if (((y1 > pymax) && (y2 > pymax)) || ((y1 < pymin) && (y2 <pymin)))
		{
			return ;
		}
		if (y1> pymax)
		{
			y1 = pymax ;
			x1 = (e->c - y1) / e->a ;
		}
		if (y1 < pymin)
		{
			y1 = pymin ;
			x1 = (e->c - y1) / e->a ;
		}
		if (y2 > pymax)
		{
			y2 = pymax ;
			x2 = (e->c - y2) / e->a ;
		}
		if (y2 < pymin)
		{
			y2 = pymin ;
			x2 = (e->c - y2) / e->a ;
		}
	}
	line(x1,y1,x2,y2);
}

/*** VORONOI.C ***/
/*** implicit parameters: nsites, sqrt_nsites, xmin, xmax, ymin, ymax,
	 : deltax, deltay (can all be estimates).
	 : Performance suffers if they are wrong; better to make nsites,
	 : deltax, and deltay too big than too small.  (?)
***/

void
voronoi(Site *(*nextsite)(void),UI3DPOINT* trilists, int *count_tri)
{
	Site * newsite, * bot, * top, * temp, * p, * v ;
	Point newintstar ;
	int pm ;
	Halfedge * lbnd, * rbnd, * llbnd, * rrbnd, * bisector ;
	Edge * e ;
	
	PQinitialize() ;
	bottomsite = (*nextsite)() ;
	out_site(bottomsite) ;
	ELinitialize() ;
	newsite = (*nextsite)() ;
	while (1)
	{
		if(!PQempty())
		{
			newintstar = PQ_min() ;
		}
		if (newsite != (Site *)NULL && (PQempty()
										|| newsite -> coord.y < newintstar.y
										|| (newsite->coord.y == newintstar.y
											&& newsite->coord.x < newintstar.x))) {/* new site is
																					  smallest */
			{
				out_site(newsite) ;
			}
			lbnd = ELleftbnd(&(newsite->coord)) ;
			rbnd = ELright(lbnd) ;
			bot = rightreg(lbnd) ;
			e = bisect(bot, newsite) ;
			bisector = HEcreate(e, le) ;
			ELinsert(lbnd, bisector) ;
			p = intersect(lbnd, bisector) ;
			if (p != (Site *)NULL)
			{
				PQdelete(lbnd) ;
				PQinsert(lbnd, p, dist(p,newsite)) ;
			}
			lbnd = bisector ;
			bisector = HEcreate(e, re) ;
			ELinsert(lbnd, bisector) ;
			p = intersect(bisector, rbnd) ;
			if (p != (Site *)NULL)
			{
				PQinsert(bisector, p, dist(p,newsite)) ;
			}
			newsite = (*nextsite)() ;
		}
		else if (!PQempty())   /* intersection is smallest */
		{
			lbnd = PQextractmin() ;
			llbnd = ELleft(lbnd) ;
			rbnd = ELright(lbnd) ;
			rrbnd = ELright(rbnd) ;
			bot = leftreg(lbnd) ;
			top = rightreg(rbnd) ;
			
			
			
			//out_triple(bot, top, rightreg(lbnd)) ;
			
			
			
			
			//out_triple(Site * s1, Site * s2, Site * s3)
			if (triangulate && !plot && !debug)
			{
				
				trilists[*count_tri].m_X = bot->sitenbr;
				trilists[*count_tri].m_Y = top->sitenbr;
				trilists[*count_tri].m_Z = rightreg(lbnd)->sitenbr;
				(*count_tri)++;
				
				//fprintf(fid_triple,"%d %d %d\n", s1->sitenbr, s2->sitenbr, s3->sitenbr) ;
				//printf("%d %d %d\n", bot->sitenbr, top->sitenbr, rightreg(lbnd)->sitenbr) ;
			}
			/*if (debug)
			  {
			  printf("circle through left=%d right=%d bottom=%d\n",
			  s1->sitenbr, s2->sitenbr, s3->sitenbr) ;
			  }
			*/
			
			
			v = lbnd->vertex ;
			makevertex(v) ;
			endpoint(lbnd->ELedge, lbnd->ELpm, v);
			endpoint(rbnd->ELedge, rbnd->ELpm, v) ;
			ELdelete(lbnd) ;
			PQdelete(rbnd) ;
			ELdelete(rbnd) ;
			pm = le ;
			if (bot->coord.y > top->coord.y)
			{
				temp = bot ;
				bot = top ;
				top = temp ;
				pm = re ;
			}
			e = bisect(bot, top) ;
			bisector = HEcreate(e, pm) ;
			ELinsert(llbnd, bisector) ;
			endpoint(e, re-pm, v) ;
			deref(v) ;
			p = intersect(llbnd, bisector) ;
			if (p  != (Site *) NULL)
			{
				PQdelete(llbnd) ;
				PQinsert(llbnd, p, dist(p,bot)) ;
			}
			p = intersect(bisector, rrbnd) ;
			if (p != (Site *) NULL)
			{
				PQinsert(bisector, p, dist(p,bot)) ;
			}
		}
		else
		{
			break ;
		}
	}
	
	for( lbnd = ELright(ELleftend) ;
		 lbnd != ELrightend ;
		 lbnd = ELright(lbnd))
	{
		e = lbnd->ELedge ;
		out_ep(e) ;
	}
	
}
