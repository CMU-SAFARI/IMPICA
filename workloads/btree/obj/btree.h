#define INPUT_SIZE (3000000)
 
#define ITEM_SIZE (50)

#define BTREE_ORDER (16)

typedef struct _stritem {
	struct _stritem * next;
	char            data[ITEM_SIZE];
} item_t;

typedef struct bt_node {
	// TODO bad hack!
	item_t *pointers[BTREE_ORDER]; // for non-leaf nodes, point to bt_nodes
	bool is_leaf;
	unsigned long keys[BTREE_ORDER];
	bt_node * parent;
	signed int num_keys;
	bt_node * next;
	bool latch;
	//pthread_mutex_t locked;
	signed int latch_type;
	unsigned int share_cnt;
} bt_node;

enum RC { RCOK, Commit, Abort, WAIT, ERROR, FINISH};

/* INDEX */
enum latch_t {LATCH_EX, LATCH_SH, LATCH_NONE};

// accessing type determines the latch type on nodes
enum idx_acc_t {INDEX_INSERT, INDEX_READ, INDEX_NONE};

typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long uint64_t;
typedef long int64_t;

typedef uint32_t UInt32;
typedef int32_t SInt32;
typedef uint64_t UInt64;
typedef int64_t SInt64;

typedef uint64_t idx_key_t; // key id for index
typedef uint64_t (*func_ptr)(idx_key_t);	// part_id func_ptr(index_key);


/************************************************/
// ASSERT Helper
/************************************************/
#define M_ASSERT(cond, str) \
	if (!(cond)) {\
		printf("ASSERTION FAILURE [%s : %d] msg:%s\n", __FILE__, __LINE__, str);\
		exit(0); \
	}
#define ASSERT(cond) assert(cond)


/*
typedef struct _intputitem {
	unsigned long   k
	int             nkey;
	int             nv;
	const char      *key;
} input_item;
*/
