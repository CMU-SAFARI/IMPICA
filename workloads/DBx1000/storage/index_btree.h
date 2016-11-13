#ifndef _BTREE_H_
#define _BTREE_H_

#include "global.h"
#include "helper.h"
#include "index_base.h"


typedef struct bt_node {
	// TODO bad hack!
   	void *pointers[BTREE_ORDER]; // for non-leaf nodes, point to bt_nodes
	bool is_leaf;
	idx_key_t keys[BTREE_ORDER];
	bt_node * parent;
	SInt32 num_keys;
	bt_node * next;
	bool latch;
	//pthread_mutex_t locked;
	SInt32 latch_type;
	UInt32 share_cnt;
} bt_node;

struct glob_param {
	uint64_t part_id;
};

class index_btree : public index_base {
public:
	RC			init(uint64_t part_cnt);
	RC			init(uint64_t part_cnt, table_t * table);
	bool 		index_exist(idx_key_t key); // check if the key exist. 
	RC 			index_insert(idx_key_t key, itemid_t * item, int part_id = -1);

	// the following call returns a single item
	RC	 		index_read(idx_key_t key, itemid_t * &item, int part_id=-1);	
	RC	 		index_read(idx_key_t key, itemid_t * &item,
							int part_id=-1, int thd_id=0);

    RC	 		index_read_issue(idx_key_t key, 
							int part_id=-1, int thd_id=0);
    RC	 		index_read_result(itemid_t * &item, int thd_id=0);

	RC 			index_next(uint64_t thd_id, itemid_t * &item, bool samekey = false);

private:
	// index structures may have part_cnt = 1 or PART_CNT.
	uint64_t part_cnt;
	RC			make_lf(uint64_t part_id, bt_node *& node);
	RC			make_nl(uint64_t part_id, bt_node *& node);
	RC		 	make_node(uint64_t part_id, bt_node *& node);
	
	RC 			start_new_tree(glob_param params, idx_key_t key, itemid_t * item);
	RC 			find_leaf(glob_param params, idx_key_t key, idx_acc_t access_type, bt_node *& leaf, bt_node  *& last_ex);
	RC 			find_leaf(glob_param params, idx_key_t key, idx_acc_t access_type, bt_node *& leaf);
	RC			insert_into_leaf(glob_param params, bt_node * leaf, idx_key_t key, itemid_t * item);
	// handle split
	RC 			split_lf_insert(glob_param params, bt_node * leaf, idx_key_t key, itemid_t * item);
	RC 			split_nl_insert(glob_param params, bt_node * node, SInt32 left_index, idx_key_t key, bt_node * right);
	RC 			insert_into_parent(glob_param params, bt_node * left, idx_key_t key, bt_node * right);
	RC 			insert_into_new_root(glob_param params, bt_node * left, idx_key_t key, bt_node * right);

	int			leaf_has_key(bt_node * leaf, idx_key_t key);
	
	SInt32 		cut(SInt32 length);
	SInt32	 	order; // # of keys in a node(for both leaf and non-leaf)
	bt_node ** 	roots; // each partition has a different root
	bt_node *   find_root(uint64_t part_id);

	bool 		latch_node(bt_node * node, latch_t latch_type);
	latch_t		release_latch(bt_node * node);
	RC		 	upgrade_latch(bt_node * node);
	// clean up all the LATCH_EX up tp last_ex
	RC 			cleanup(bt_node * node, bt_node * last_ex);

	// the leaf and the idx within the leaf that the thread last accessed.
	bt_node *** cur_leaf_per_thd;
	SInt32 ** 		cur_idx_per_thd;
	
	// latch for the whole tree
	bool        tree_latch;
	latch_t     tree_latch_type;
	UInt32      tree_share_cnt;

	bool 		latch_tree(latch_t latch_type);
	latch_t		release_tree_latch();
	RC		 	upgrade_tree_latch();


};

#endif
