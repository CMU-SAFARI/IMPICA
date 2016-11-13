#define INPUT_SIZE (1572864)
#define MAX_KEY_SIZE (120)


#define hashsize(n) ((unsigned long)1<<(n))
#define hashmask(n) (hashsize(n)-1)

typedef struct _stritem {
    struct _stritem *h_next;    /* hash chain next */
    int             nkey;       /* size of key */
	char            data[MAX_KEY_SIZE];
} item;

typedef struct _intputitem {
	int             nkey;
	int             nv;
	const char      *key;
} input_item;
