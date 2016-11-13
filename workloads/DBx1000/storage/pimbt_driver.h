#include <linux/ioctl.h>

/* 
 * The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. 
 */
#define MAJOR_NUM 70

/*
 *
 * Define the IOCTL message between user code and driver 
 *
 */
#define IOCTL_START_TRAVERSE  _IOR(MAJOR_NUM, 1, unsigned long *)

#define IOCTL_POLL_DONE       _IOWR(MAJOR_NUM, 2, unsigned long)
