#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include "hashtable.h"

#define TEST_ITERATION 100000

#define GEM5
#define PIM

#ifdef GEM5
#include "m5op.h"
#endif

int g_pimbt_dev = -1;
char *g_pim_register = NULL;

/* how many powers of 2's worth of buckets we use */
unsigned int hashpower = 20;

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
static const int KeySize           = 0x038;

/** Channel mask **/
static const int ChannelShft       = 8;

/** Huge input table. We do this to speed up the simulation time. Otherwise
    we need to read it from files and that's slow **/
extern input_item input_table[];

/** Functions for register access **/
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

void grab_pim_table()
{
	unsigned long baseaddr = (unsigned long)g_pim_register;
	
	iowrite32(1, baseaddr + GrabPageTable);

	// Polling for done
	while (ioread32(baseaddr + GrabPageTable) != 2);
}


/* Main hash table. This is where we look except during expansion. */
static item** primary_hashtable = 0;

/* The memory pool for the items in the hash table */
static item* item_memory_pool = 0;


void assoc_init() {
	primary_hashtable = (item**) calloc(hashsize(hashpower), sizeof(void *));	
}

item *assoc_find(const char *key, const int nkey, const int hv) {
    item *it;
    
    item *ret = NULL;

	it = primary_hashtable[hv & hashmask(hashpower)];

#if defined(PIM)

	unsigned long baseaddr = (unsigned long)g_pim_register;
	unsigned int done;
	
	iowrite64((unsigned long)it, baseaddr + RootAddrLo);
	iowrite32((unsigned int)nkey, baseaddr + KeySize);
	iowrite64((unsigned long)key, baseaddr + KeyLo);

	// Polling for done
	do {
		done = ioread32(baseaddr + Done);
	} while (done != 1);

	// Read the pointer to the node
	ret = (item *)ioread64(baseaddr + LeafLo);

#else		

	int depth = 0;

    while (it) {
        if ((nkey == it->nkey) && (memcmp(key, it->data, nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
        ++depth;
    }

#endif

	assert(ret!=NULL);
    return ret;
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int assoc_insert(item *it, const int hv) {

	it->h_next = primary_hashtable[hv & hashmask(hashpower)];
	primary_hashtable[hv & hashmask(hashpower)] = it;

    return 1;
}

int main(int argc, char *argv[]) {

	/* initialize the hash table */
	assoc_init();

	/* Allocate the memory for all items */
	item_memory_pool = (item *)malloc(sizeof(item) * INPUT_SIZE);

#if defined(PIM)	
	// open PIMBT device
	g_pimbt_dev = open("/dev/pimbt", O_RDWR);

    if (g_pimbt_dev<0)
		printf("Error opening file \n");

	g_pim_register = (char *)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED,
										g_pimbt_dev, 0);
	if (g_pim_register == MAP_FAILED)
		printf ("Map PIM register failed\n");
#endif


	/* Insert everything to the hash table */
	int i, test_idx;
	item *it;
	for (i = 0; i < INPUT_SIZE; i++) {
		it = item_memory_pool++;
		it->nkey = input_table[i].nkey;
		memcpy(it->data, input_table[i].key, input_table[i].nkey);
		assoc_insert(it, input_table[i].nv);
	}

#if defined(GEM5)
	m5_checkpoint(0, 0);

#if defined(PIM)
    grab_pim_table();
#endif

	m5_reset_stats(0, 0);
#endif


	/* Do the test randomly */
	for (i = 0; i < TEST_ITERATION; i++) {
		test_idx = rand() % INPUT_SIZE;
		it = assoc_find(input_table[test_idx].key, 
						input_table[test_idx].nkey,
						input_table[test_idx].nv);
	}

#if defined(GEM5)
	m5_dump_stats(0, 0);
	m5_exit(0);
#endif	
	
	return 0;
}

