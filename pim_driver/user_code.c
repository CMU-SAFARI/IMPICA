#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "pimbt_driver.h"

unsigned int count=16;

int root_node[8] = {20, 21, 22, 23, 24, 25, 26, 27};
volatile unsigned long *pim_register;

static inline void iowrite32(unsigned int b, volatile void *addr)
{
	*(volatile unsigned int *) addr = b;
}

static inline unsigned int ioread32(const volatile void *addr)
{
	return *(const volatile unsigned int *) addr;
}

static inline void iowrite64(unsigned long b, volatile void *addr)
{
	*(volatile unsigned long *) addr = b;
}

static inline unsigned long ioread64(const volatile void *addr)
{
	return *(const volatile unsigned long *) addr;
}


int main()
{
    //FILE* dev;
	int dev;
    int i;
	unsigned long retval;

    dev = open("/dev/pimbt", O_RDWR);

    if (dev<0)
		printf("Error opening file \n");

#if 0
	
	unsigned long buf[3] = {(unsigned long)root_node, 0x8000, 2};

	printf ("Start traverse: %lx, %lx, %lx\n", buf[0], buf[1], buf[2]);

	retval = ioctl(dev, IOCTL_START_TRAVERSE, buf);

	buf[0] = 2;
	buf[1] = -1;

	retval = ioctl(dev, IOCTL_POLL_DONE, buf);

	printf ("Poll done ret %d, buf: %lx, %lx\n", retval, buf[0], buf[1]);

#else

	pim_register = (unsigned long *)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED,
										dev, 0);
	if (pim_register == MAP_FAILED)
		printf ("Map PIM register failed\n");

	iowrite64(0x888888889999999, pim_register);
	//	iowrite64(0x99999999, pim_register + 1);

	retval = ioread64(pim_register);

	printf("Read data %lx", retval);

#endif

	munmap((void *)pim_register, 0x1000);
    close(dev);

	//	while(1){};
}
