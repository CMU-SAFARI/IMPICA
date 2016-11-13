/*
 * LLUBENCHMARK 
 * Craig Zilles (zilles@cs.wisc.edu)
 * http://www.cs.wisc.edu/~zilles/llubenchmark.html
 *
 * This program is a linked list traversal micro-benchmark, which can
 * be used (among other things) to approximate the non-benchmark
 * Health.
 * 
 * The benchmark executes for a proscribed number of iterations (-i),
 * and on every iteration the lists are traversed and potentially
 * extended.  The number of lists can be specified (-n) as well as the
 * size of the elements in the list (-s).  The initial length of the
 * lists can be set (-l) as well as the growth rate (-g).  The growth
 * rate must be non-negative, but can be a floating point number, in
 * which case random numbers are used to determine whether a list is
 * extended on a particular cycle (all lists are extended
 * independently).  If the -t option is specified, the insertion
 * occurs at the tail, otherwise at the head.  If the -d option is
 * specified, the elements are dirtied during the traversal (which
 * will necessitate a write-back when the data is evicted from the
 * cache).
 *
 * To approximate the non-benchmark Health, use the options:
 *     -i <num iterations> -g .333 -d -t -n 341
 * 
 * (the growth rate of the lists in health is different for different
 * levels of the hierarchy and the constant .333 is just my
 * approximation of the growth rate).
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>


static inline void iowrite32(unsigned int b, unsigned long addr)
{
	*(volatile unsigned int *) addr = b;
}

static inline unsigned int ioread32(const unsigned long addr)
{
	return *(const volatile unsigned int *) addr;
}

static inline void iowrite64(unsigned long b, unsigned long addr)
{
	*(volatile unsigned long *) addr = b;
}

static inline unsigned long ioread64(const unsigned long addr)
{
	return *(const volatile unsigned long *) addr;
}

#define GEM5
#define PIM

#ifdef GEM5
#include "m5op.h"
#endif

int g_pimbt_dev = -1;
char *g_pim_register = NULL;


/* This file should compile stand alone */

struct element {
  struct element *next;
  int count;
};

void
usage(char *name) {
  printf("%s:\n", name);
  printf("-i <number of (I)terations>\n");
  printf("[-l <initial (L)ength of list, in elements>] (default 1)\n");
  printf("[-n <(N)umber of lists>] (default 1 list)\n");
  printf("[-s <(S)ize of element>] (default 32 bytes)\n");
  printf("[-g <(G)rowth rate per list, in elements per iteration>] (default 0)\n");
  printf("[-d] ((D)irty each element during traversal, default off)\n");
  printf("[-t] (insert at (T)ail of list, default off)\n");
}

#define ALLOC_SIZE 10230 /* pick wierd num to break strides */
struct element *free_list = NULL;
int next_free = ALLOC_SIZE;
int element_size = 64;
int num_allocated = 0;

/** Register map*/
static const int RootAddrLo        = 0x000;
static const int RootAddrHi        = 0x004;
static const int PageTableLo       = 0x008;
static const int PageTableHi       = 0x00C;
static const int KeyLo             = 0x010;
static const int KeyHi             = 0x014;
static const int StartTrav         = 0x018;
static const int Done              = 0x01C;
static const int ItemLo            = 0x020;
static const int ItemHi            = 0x024;
static const int LeafLo            = 0x028;
static const int LeafHi            = 0x02C;
static const int ItemIndex         = 0x030;
static const int GrabPageTable     = 0x034;

/** Channel mask **/
static const int ChannelShft       = 8;


struct element *
allocate() {
  if (next_free == ALLOC_SIZE) {
	 next_free = 0;
	 free_list = (struct element *) malloc (ALLOC_SIZE * element_size);
	 assert(free_list != 0);
  }
  num_allocated ++;
  return (struct element *) 
	 (((char *)free_list) + ((next_free ++) * element_size));
}

void grab_pim_table()
{
	unsigned long baseaddr = (unsigned long)g_pim_register;
	
	iowrite32(1, baseaddr + GrabPageTable);

	// Polling for done
	while (ioread32(baseaddr + GrabPageTable) != 2);
}


int
main(int argc, char *argv[]) {
  int max_iterations = 0,
	 dirty = 0,
	 num_lists = 1,
	 tail = 0,
	 initial_length = 1;
  float growth_rate = 0.0;  
  char c = 0;
  int i = 0, j = 0, k = 0;
  int accumulate = 0;

  struct element **lists = NULL;
  float growth = 0.0;

  int arg = 1;
  while (arg < argc) {
	 if ((argv[arg][0] != '-') || (argv[arg][2] != 0)) {
		printf("parse error in %s\n", argv[arg]);
		usage(argv[0]);
		return(-1);
	 }
	 c = argv[arg][1];
	 arg ++;
	 switch(c) {
	 case 'd': 		dirty = 1; break;
	 case 'g': 		growth_rate = atof(argv[arg++]);  break;
	 case 'i': 		max_iterations = atoi(argv[arg++]); break;
	 case 'l': 		initial_length = atoi(argv[arg++]); break;
	 case 'n': 		num_lists = atoi(argv[arg++]); break;
	 case 's': 		element_size = atoi(argv[arg++]); break;
	 case 't': 		tail = 1; break;
	 default:
		printf("unrecognized option: %c\n", c);
		usage(argv[0]);
		return(-1);
	 }
  }
		
  assert (element_size > sizeof(struct element));
  assert (initial_length > 0);

#if defined(GEM5)	
	// open PIMBT device
	g_pimbt_dev = open("/dev/pimbt", O_RDWR);

    if (g_pimbt_dev<0)
		printf("Error opening file \n");

	g_pim_register = (char *)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED,
										g_pimbt_dev, 0);
	if (g_pim_register == MAP_FAILED)
		printf ("Map PIM register failed\n");
#endif


  /* build lists */
  lists = (struct element **) malloc (num_lists * sizeof(struct element *));
  assert(lists != 0);
  
  for (i = 0 ; i < num_lists ; i ++) { 
	 lists[i] = NULL;
  }


  for (i = 0 ; i < initial_length ; i ++) { 
	 for (j = 0 ; j < num_lists ; j ++) { 
		struct element *e = allocate();
		e->next = lists[j];
		lists[j] = e;
	 }
  }

#if defined(GEM5)
	m5_checkpoint(0, 0);

#if defined(PIM)
    grab_pim_table();
#endif

	m5_reset_stats(0, 0);
#endif


  /* iterate */
  for (i = 0 ; i < max_iterations ; i ++) { 
	 if ((i % 10) == 0) {
		printf("%d\n", i);
	 }
	 /* traverse lists */
	 for (j = 0 ; j < num_lists ; j ++) { 
		struct element *trav = lists[j];

#if defined(PIM)

		unsigned long baseaddr = (unsigned long)g_pim_register;
		unsigned int done;

		iowrite64((unsigned long)trav, baseaddr + KeyLo);

		// Polling for done
		do {
			done = ioread32(baseaddr + Done);
		} while (done != 1);

#else		

		while (trav != NULL) {
		  accumulate += trav->count;
		  if (dirty) {
			 trav->count ++;
		  }
		  trav = trav->next;
		}

#endif
	 }
	 
	 /* grow lists */
	 growth += growth_rate;
	 j = growth;
	 growth -= j;
	 for ( ; j > 0 ; j --) {
		for (k = 0 ; k < num_lists ; k ++) { 
		  struct element *e = allocate();
		  if (tail) {

#if defined(PIM)

			  struct element *trav = lists[k], *tail_ptr;

			  unsigned long baseaddr = (unsigned long)g_pim_register;
			  unsigned int done;

			  iowrite64((unsigned long)trav, baseaddr + KeyLo);

			  // Polling for done
			  do {
				  done = ioread32(baseaddr + Done);
			  } while (done != 1);

			  tail_ptr = (struct element *)ioread64(baseaddr + LeafLo);

			  tail_ptr->next = e;
			  e->next = NULL;


#else		

			 struct element *trav = lists[k];
			 while (trav->next != NULL) {
				trav = trav->next;
			 }
			 trav->next = e;
			 e->next = NULL;

#endif

		  } else {
			 e->next = lists[k];
			 lists[k] = e;
		  }
		}
	 }
  }

#if defined(GEM5)
	m5_dump_stats(0, 0);
	m5_exit(0);
#endif	

	//printf ("num allocated %d\n", num_allocated);
  return 0;
}
	 
