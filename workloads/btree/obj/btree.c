#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include "btree.h"

#define TEST_ITERATION 1000000

//#define GEM5
//#define PIM

#ifdef GEM5
#include "m5op.h"
#endif

int g_pimbt_dev = -1;
char *g_pim_register = NULL;
int btree_node = 0;

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
extern unsigned long input_table[];

bt_node * 	tree_root; 

SInt32	 	order = BTREE_ORDER; // # of keys in a node(for both leaf and non-leaf)

// the leaf and the idx within the leaf that the thread last accessed.
bt_node * cur_leaf_per_thd;
SInt32    cur_idx_per_thd;


/* The memory pool for the items in the hash table */
static item_t* item_memory_pool = 0;

/* The memory pool for the nodes in the B-tree */
static bt_node* node_memory_pool = 0;



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


/* Prototypes */
SInt32 cut(SInt32 length);
RC insert_into_parent(bt_node * left, idx_key_t key, bt_node * right);
RC find_leaf(idx_key_t key, idx_acc_t access_type, bt_node *& leaf);
RC find_leaf_ex(idx_key_t key, idx_acc_t access_type, bt_node *& leaf, bt_node *& last_ex);
RC split_nl_insert(bt_node * old_node, SInt32 left_index, idx_key_t key, bt_node * right);
RC insert_into_new_root(bt_node * left, idx_key_t key, bt_node * right);
int leaf_has_key(bt_node * leaf, idx_key_t key);
RC insert_into_leaf(bt_node * leaf, idx_key_t key, item_t * item);
RC split_lf_insert(bt_node * leaf, idx_key_t key, item_t * item);

RC make_node(bt_node *& node) {	

	bt_node * new_node = node_memory_pool++;
	btree_node++;
	assert (new_node != NULL);
	//new_node->pointers = NULL;
	//new_node->keys = (idx_key_t *) mem_allocator.alloc((order - 1) * sizeof(idx_key_t), part_id);
	//new_node->pointers = (void **) mem_allocator.alloc(order * sizeof(void *), part_id);
	//assert (new_node->keys != NULL && new_node->pointers != NULL);
	new_node->is_leaf = false;
	new_node->num_keys = 0;
	new_node->parent = NULL;
	new_node->next = NULL;
//	new_node->locked = false;
	new_node->latch = false;
	new_node->latch_type = LATCH_NONE;

	node = new_node;
	return RCOK;
}


RC make_lf(bt_node *& node) {
	RC rc = make_node(node);
	if (rc != RCOK) return rc;
	node->is_leaf = true;
	return RCOK;
}

RC make_nl(bt_node *& node) {
	RC rc = make_node(node);
	if (rc != RCOK) return rc;
	node->is_leaf = false;
	return RCOK;
}

RC btree_init() {
	RC rc;
	rc = make_lf(tree_root);
	assert (rc == RCOK);

	return RCOK;
}


bool latch_node(bt_node * node, latch_t latch_type) {
	// TODO latch is disabled 
	return true;
}

latch_t release_latch(bt_node * node) {
	// TODO latch is disabled 
	return LATCH_SH;
}

RC upgrade_latch(bt_node * node) {
	// TODO latch is disabled 
	return RCOK;
}

RC cleanup(bt_node * node, bt_node * last_ex) {
	if (last_ex != NULL) {
		do {
			node = node->parent;
			release_latch(node);
		}
		while (node != last_ex);
	}
	return RCOK;
}


#if (!BTREE_PIM)

RC index_read(idx_key_t key, item_t *& item) 
{
	RC rc = Abort;
	bt_node * leaf;
	
	//if (!latch_tree(LATCH_SH))
	//	return Abort;

	find_leaf(key, INDEX_READ, leaf);
	if (leaf == NULL)
		M_ASSERT(false, "the leaf does not exist!");
	for (SInt32 i = 0; i < leaf->num_keys; i++) 
		if (leaf->keys[i] == key) {
			item = (item_t *)leaf->pointers[i];
			release_latch(leaf);
			cur_leaf_per_thd = leaf;
			cur_idx_per_thd = i;
			rc = RCOK;
		}
	// release the latch after reading the node

	//printf("key = %ld\n", key);
	if (rc == RCOK) {
	   	//return rc;
		//itemid_t *pim_item;
		//index_read_pim(key, pim_item, part_id, thd_id);
		//M_ASSERT(pim_item == item, "SW and HW traverse into different item");		
	} else {
		M_ASSERT(false, "the key does not exist!");
	}

	//release_tree_latch();

	return rc;
}

#else    //BTREE_PIM

RC index_read(idx_key_t key, itemid_t *& item, 
	int part_id, int thd_id) 
{
	unsigned long baseaddr = (unsigned long)g_pim_register + (thd_id << 8); 
	unsigned int done;

	unsigned long c = tree_root;

	assert(c != 0);

	//if (!latch_tree(LATCH_SH))
	//	return Abort;

	iowrite64(c, baseaddr + RootAddrLo);
	iowrite64(key, baseaddr + KeyLo);

	// Polling for done
	do {
		done = ioread32(baseaddr + Done);
	} while (done != 1);

	item = (item_t *)ioread64(baseaddr + ItemLo);

	cur_idx_per_thd = (int)ioread32(baseaddr + ItemIndex);


#if 0
	// DEBUG DEBUG
	if (*cur_idx_per_thd[thd_id] == (int)0xFFFF) {
		find_leaf(params, key, INDEX_READ, leaf);
		if (leaf == NULL)
			M_ASSERT(false, "the leaf does not exist!");
		printf("DEBUG: leaf address %lx at ch %d\n", (unsigned long)leaf, thd_id);
		for (SInt32 i = 0; i < leaf->num_keys; i++) {
			printf("DEBUG: leaf key[%d] = %lx at ch %d\n", i, leaf->keys[i], thd_id);
			if (leaf->keys[i] == key) {
				item = (itemid_t *)leaf->pointers[i];
				release_latch(leaf);
			}
	    }
		// DEBUG DEBUG
		buf[0] = (unsigned long)leaf;
		retval = ioctl(g_pimbt_dev, IOCTL_POLL_DONE, (unsigned long)buf);
		if (retval != 1) {
			printf("DEBUG: the return value of DEVICE IOCTL is wrong!!\n");
		}
		
		assert(0);
	}

	lo = ioread32(baseaddr + LeafLo);
	hi = ioread32(baseaddr + LeafHi);

	(*cur_leaf_per_thd[thd_id]) = (bt_node*) (hi << 32 | lo);

#else

	(*cur_leaf_per_thd[thd_id]) = (bt_node*) ioread64(baseaddr + LeafLo);

#endif

	//release_tree_latch();

	return RCOK;
}

#endif //BTREE_PIM

RC find_leaf(idx_key_t key, idx_acc_t access_type, bt_node *& leaf) {
	bt_node * last_ex = NULL;
	assert(access_type != INDEX_INSERT);
	RC rc = find_leaf_ex(key, access_type, leaf, last_ex);
	return rc;
}

RC index_insert(idx_key_t key, item_t * item) {
	// create a tree if there does not exist one already
	RC rc = RCOK;
	bt_node * root = tree_root;
	assert(root != NULL);
	int depth = 0;
	// TODO tree depth < 100
	bt_node * ex_list[100];
	bt_node * leaf = NULL;
	bt_node * last_ex = NULL;

	// Latch the whole tree
	//while (!latch_tree(LATCH_SH));

	rc = find_leaf_ex(key, INDEX_INSERT, leaf, last_ex);
	assert(rc == RCOK);
	
	bt_node * tmp_node = leaf;
	if (last_ex != NULL) {
		while (tmp_node != last_ex) {
	//		assert( tmp_node->latch_type == LATCH_EX );
			ex_list[depth++] = tmp_node;
			tmp_node = tmp_node->parent;
			assert (depth < 100);
		}
		ex_list[depth ++] = last_ex;
	} else
		ex_list[depth++] = leaf;
	// from this point, the required data structures are all latched,
	// so the system should not abort anymore.
//	M_ASSERT(!index_exist(key), "the index does not exist!");
	// insert into btree if the leaf is not full

	// We need to upgrade the latch for the whole tree
	//while (RCOK != upgrade_tree_latch());

	if (leaf->num_keys < order - 1 || leaf_has_key(leaf, key) >= 0) {
		rc = insert_into_leaf(leaf, key, item);
		// only the leaf should be ex latched.
//		assert( release_latch(leaf) == LATCH_EX );
		for (int i = 0; i < depth; i++)
			release_latch(ex_list[i]);
//			assert( release_latch(ex_list[i]) == LATCH_EX );
	}
	else { // split the nodes when necessary
		rc = split_lf_insert(leaf, key, item);
		for (int i = 0; i < depth; i++)
			release_latch(ex_list[i]);
//			assert( release_latch(ex_list[i]) == LATCH_EX );
	}
//	assert(leaf->latch_type == LATCH_NONE);

//    release_tree_latch();

	return rc;
}



RC find_leaf_ex(idx_key_t key, idx_acc_t access_type, bt_node *& leaf, bt_node *& last_ex) 
{
//	RC rc;
	SInt32 i;
	bt_node * c = tree_root;
	assert(c != NULL);
	bt_node * child;

	if (access_type == INDEX_NONE) {
		while (!c->is_leaf) {
			for (i = 0; i < c->num_keys; i++) {
				if (key < c->keys[i])
					break;
			}
			c = (bt_node *)c->pointers[i];
		}
		leaf = c;
		return RCOK;
	}

	while (!c->is_leaf) {
		for (i = 0; i < c->num_keys; i++) {
			if (key < c->keys[i])
				break;
		}
		child = (bt_node *)c->pointers[i];
		if (!latch_node(child, LATCH_SH)) {
			release_latch(c);
			cleanup(c, last_ex);
			last_ex = NULL;
			return Abort;
		}	
		if (access_type == INDEX_INSERT) {
			if (child->num_keys == order - 1) {
				if (upgrade_latch(c) != RCOK) {
					release_latch(c);
					release_latch(child);
					cleanup(c, last_ex);
					last_ex = NULL;
					return Abort;
				}
				if (last_ex == NULL)
					last_ex = c;
			}
			else { 
				cleanup(c, last_ex);
				last_ex = NULL;
				release_latch(c);
			}
		} else
			release_latch(c); // release the LATCH_SH on c
		c = child;
	}
	// c is leaf		
	// at this point, if the access is a read, then only the leaf is latched by LATCH_SH
	// if the access is an insertion, then the leaf is sh latched and related nodes in the tree
	// are ex latched.
	if (access_type == INDEX_INSERT) {
		if (upgrade_latch(c) != RCOK) {
        	release_latch(c);
            cleanup(c, last_ex);
            return Abort;
        }
	}
	leaf = c;
	assert (leaf->is_leaf);
	return RCOK;
}

RC insert_into_leaf(bt_node * leaf, idx_key_t key, item_t * item) {
	SInt32 i, insertion_point;
    insertion_point = 0;
	int idx = leaf_has_key(leaf, key);	
	if (idx >= 0) {
		item->next = (item_t *)leaf->pointers[idx];
		leaf->pointers[idx] =  item;
		return RCOK;
	}
    while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key)
        insertion_point++;
	for (i = leaf->num_keys; i > insertion_point; i--) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->pointers[i] = leaf->pointers[i - 1];
    }
    leaf->keys[insertion_point] = key;
    leaf->pointers[insertion_point] = item;
    leaf->num_keys++;
	M_ASSERT( (leaf->num_keys < order), "too many keys in leaf" );
    return RCOK;
}

RC split_lf_insert(bt_node * leaf, idx_key_t key, item_t * item) {
    RC rc;
	SInt32 insertion_index, split, i, j;
	idx_key_t new_key;

    bt_node * new_leaf;
//	printf("will make_lf(). part_id=%lld, key=%lld\n", part_id, key);
//	pthread_t id = pthread_self();
//	printf("%08x\n", id);
	rc = make_lf(new_leaf);
	if (rc != RCOK) return rc;

	M_ASSERT(leaf->num_keys == order - 1, "trying to split non-full leaf!");

	idx_key_t temp_keys[BTREE_ORDER];
	item_t * temp_pointers[BTREE_ORDER];
    insertion_index = 0;
    while (insertion_index < order - 1 && leaf->keys[insertion_index] < key)
        insertion_index++;

    for (i = 0, j = 0; i < leaf->num_keys; i++, j++) {
        if (j == insertion_index) j++;
//		new_leaf->keys[j] = leaf->keys[i];
//		new_leaf->pointers[j] = (item_t *)leaf->pointers[i];
        temp_keys[j] = leaf->keys[i];
        temp_pointers[j] = (item_t *)leaf->pointers[i];
    }
//	new_leaf->keys[insertion_index] = key;
//	new_leaf->pointers[insertion_index] = item;
    temp_keys[insertion_index] = key;
    temp_pointers[insertion_index] = item;
	
   	// leaf is on the left of new_leaf
    split = cut(order - 1);
    leaf->num_keys = 0;
    for (i = 0; i < split; i++) {
//        leaf->pointers[i] = new_leaf->pointers[i];
//        leaf->keys[i] = new_leaf->keys[i];
        leaf->pointers[i] = temp_pointers[i];
        leaf->keys[i] = temp_keys[i];
        leaf->num_keys++;
		M_ASSERT( (leaf->num_keys < order), "too many keys in leaf" );
    }
	for (i = split, j = 0; i < order; i++, j++) {
//        new_leaf->pointers[j] = new_leaf->pointers[i];
//        new_leaf->keys[j] = new_leaf->keys[i];
        new_leaf->pointers[j] = temp_pointers[i];
        new_leaf->keys[j] = temp_keys[i];
        new_leaf->num_keys++;
		M_ASSERT( (leaf->num_keys < order), "too many keys in leaf" );
    }
	
//    delete temp_pointers;
//    delete temp_keys;

	new_leaf->next = leaf->next;
	leaf->next = new_leaf;
	
//    new_leaf->pointers[order - 1] = leaf->pointers[order - 1];
//    leaf->pointers[order - 1] = new_leaf;

    for (i = leaf->num_keys; i < order - 1; i++)
        leaf->pointers[i] = NULL;
    for (i = new_leaf->num_keys; i < order - 1; i++)
        new_leaf->pointers[i] = NULL;

    new_leaf->parent = leaf->parent;
    new_key = new_leaf->keys[0];
	
    rc = insert_into_parent(leaf, new_key, new_leaf);
	return rc;
}

RC insert_into_parent(
	bt_node * left, 
	idx_key_t key, 
	bt_node * right) {
	
    bt_node * parent = left->parent;

    /* Case: new root. */
    if (parent == NULL)
        return insert_into_new_root(left, key, right);
    
	SInt32 insert_idx = 0;
	while (parent->keys[insert_idx] < key && insert_idx < parent->num_keys)
		insert_idx ++;
	// the parent has enough space, just insert into it
    if (parent->num_keys < order - 1) {
		for (SInt32 i = parent->num_keys-1; i >= insert_idx; i--) {
			parent->keys[i + 1] = parent->keys[i];
			parent->pointers[i+2] = parent->pointers[i+1];
		}
		parent->num_keys ++;
		parent->keys[insert_idx] = key;
		parent->pointers[insert_idx + 1] = (item_t *)right;
		return RCOK;
	}

    /* Harder case:  split a node in order 
     * to preserve the B+ tree properties.
     */
	
	return split_nl_insert(parent, insert_idx, key, right);
//	return RCOK;
}

RC insert_into_new_root(
	bt_node * left, idx_key_t key, bt_node * right) 
{
	RC rc;
	bt_node * new_root;
//	printf("will make_nl(). part_id=%lld. key=%lld\n", part_id, key);
	rc = make_nl(new_root);
	if (rc != RCOK) return rc;
    new_root->keys[0] = key;
    new_root->pointers[0] = (item_t *)left;
    new_root->pointers[1] = (item_t *)right;
    new_root->num_keys++;
	M_ASSERT( (new_root->num_keys < order), "too many keys in leaf" );
    new_root->parent = NULL;
    left->parent = new_root;
    right->parent = new_root;
	left->next = right;

	tree_root = new_root;	
	// TODO this new root is not latched, at this point, other threads
	// may start to access this new root. Is this ok?
    return RCOK;
}

RC split_nl_insert(
	bt_node * old_node, 
	SInt32 left_index, 
	idx_key_t key, 
	bt_node * right) 
{
	RC rc;
	int64_t i, j, split, k_prime;
    bt_node * new_node, * child;
//    idx_key_t * temp_keys;
//    btUSInt32 temp_pointers;
    rc = make_node(new_node);

    /* First create a temporary set of keys and pointers
     * to hold everything in order, including
     * the new key and pointer, inserted in their
     * correct places. 
     * Then create a new node and copy half of the 
     * keys and pointers to the old node and
     * the other half to the new.
     */

    idx_key_t temp_keys[BTREE_ORDER];
    bt_node * temp_pointers[BTREE_ORDER + 1];
    for (i = 0, j = 0; i < old_node->num_keys + 1; i++, j++) {
        if (j == left_index + 1) j++;
//		new_node->pointers[j] = (bt_node *)old_node->pointers[i];
        temp_pointers[j] = (bt_node *)old_node->pointers[i];
    }

    for (i = 0, j = 0; i < old_node->num_keys; i++, j++) {
        if (j == left_index) j++;
//		new_node->keys[j] = old_node->keys[i];
        temp_keys[j] = old_node->keys[i];
    }

//    new_node->pointers[left_index + 1] = right;
//    new_node->keys[left_index] = key;
    temp_pointers[left_index + 1] = right;
    temp_keys[left_index] = key;

	/* Create the new node and copy
     * half the keys and pointers to the
     * old and half to the new.
     */
    split = cut(order);
//	printf("will make_node(). part_id=%lld, key=%lld\n", part_id, key);
	if (rc != RCOK) return rc;

    old_node->num_keys = 0;
    for (i = 0; i < split - 1; i++) {
//        old_node->pointers[i] = new_node->pointers[i];
//        old_node->keys[i] = new_node->keys[i];
        old_node->pointers[i] = (item_t *)temp_pointers[i];
        old_node->keys[i] = temp_keys[i];
        old_node->num_keys++;
		M_ASSERT( (old_node->num_keys < order), "too many keys in leaf" );
    }

	new_node->next = old_node->next;
	old_node->next = new_node;

    old_node->pointers[i] = (item_t *)temp_pointers[i];
    k_prime = temp_keys[split - 1];
//    old_node->pointers[i] = new_node->pointers[i];
//    k_prime = new_node->keys[split - 1];
    for (++i, j = 0; i < order; i++, j++) {
        new_node->pointers[j] = (item_t *)temp_pointers[i];
        new_node->keys[j] = temp_keys[i];
//        new_node->pointers[j] = new_node->pointers[i];
//        new_node->keys[j] = new_node->keys[i];
        new_node->num_keys++;
		M_ASSERT( (old_node->num_keys < order), "too many keys in leaf" );
    }
    new_node->pointers[j] = (item_t *)temp_pointers[i];
//    new_node->pointers[j] = new_node->pointers[i];
//    delete temp_pointers;
//    delete temp_keys;
    new_node->parent = old_node->parent;
    for (i = 0; i <= new_node->num_keys; i++) {
        child = (bt_node *)new_node->pointers[i];
        child->parent = new_node;
    }

    /* Insert a new key into the parent of the two
     * nodes resulting from the split, with
     * the old node to the left and the new to the right.
     */

    return insert_into_parent(old_node, k_prime, new_node);	
}

int leaf_has_key(bt_node * leaf, idx_key_t key) {
	for (SInt32 i = 0; i < leaf->num_keys; i++) 
		if (leaf->keys[i] == key)
			return i;
	return -1;
}

SInt32 cut(SInt32 length) {
	if (length % 2 == 0)
        return length/2;
    else
        return length/2 + 1;
}


int main(int argc, char *argv[])
{
	/* Allocate the memory for all items */
	item_memory_pool = (item_t *)calloc(sizeof(item_t), INPUT_SIZE);

	/* Allocate the memory for all nodes, including root node */
	node_memory_pool = (bt_node *)calloc(sizeof(bt_node), (INPUT_SIZE + 1));	

	/* initialize the B-tree */
	btree_init();


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


	/* Insert everything to the B-tree */
	int i, test_idx;
	item_t *it;
	for (i = 0; i < INPUT_SIZE; i++) {
		it = item_memory_pool++;
		index_insert(input_table[i], it);
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
		index_read(input_table[test_idx], it);
	}

#if defined(GEM5)
	m5_dump_stats(0, 0);
	m5_exit(0);
#endif	

	printf("Total B-tree nodes: %d\n", btree_node);
	
	return 0;
}

