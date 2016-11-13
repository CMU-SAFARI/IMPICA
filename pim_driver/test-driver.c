#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <asm/io.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#include "pimbt_driver.h"
//#include <linux/config.h>


#define BASE_PIM_DEV    (0xe000)
#define SIZE_PIM_DEV    (0x1000)

static int __init device_init(void);
void device_exit(void);
int device_open(struct inode *inode, struct file* filep);
int device_release(struct inode *inode, struct file* filep);
ssize_t device_read(struct file* filp, char* buf, size_t count,loff_t* f_pos);
ssize_t device_write(struct file* filp, const char __user * buf, size_t count, loff_t* f_pos);
long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
int device_mmap(struct file *file, struct vm_area_struct *vma);

//static int __iomem* pim_dev_membase;
static struct cdev pim_cdev;

struct file_operations device_fops = {
	//.read = device_read,
	//.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release,
	.mmap = device_mmap,
};

static int major = MAJOR_NUM; 

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
static const int ItemIndex         = 0x028;
static const int LeafLo            = 0x02C;
static const int LeafHi            = 0x030;

/** Channel mask **/
static const int ChannelShft       = 8;

static int __init device_init(void)
{

#if 0
    int check;
    check=register_chrdev(major,"/dev/pimbt",&device_fops);
    if(check<0) {
		printk("problem in registering PIM module\n");
		return check;
	}
    printk("PimBT is succefully registered!\n");

#else

	int res;
	dev_t devno = MKDEV(major, 0);
	
	cdev_init(&pim_cdev, &device_fops);
	pim_cdev.owner = THIS_MODULE;

	res = register_chrdev_region(devno, 1, "/dev/pimbt");

	if (res) {
		printk("Can't get major %d for PIM\n", major);
		return res;
	}

	res = cdev_add(&pim_cdev, devno, 1);
	if (res) {
		printk("Error %d adding PIM\n", res);
		unregister_chrdev_region(devno, 1);
		return res;
	}

#endif

#if 1
	if (! request_region (BASE_PIM_DEV, SIZE_PIM_DEV, "/dev/pimbt")) {
		printk("request region failed");
		cdev_del(&pim_cdev);
		unregister_chrdev_region(devno, 1);
		return -ENODEV;
	}
#endif

	//	pim_dev_membase = ioremap(BASE_PIM_DEV, SIZE_PIM_DEV);

	//printk("IO remap address for PIM = %lx\n", (unsigned long)pim_dev_membase);

    return 0;
}

void device_exit(void)
{
    release_region(BASE_PIM_DEV, SIZE_PIM_DEV);

	cdev_del(&pim_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);

    printk("PimBT is removed! \n");
}

int device_open(struct inode *inode, struct file* filep)
{
    printk("PimBT is opened!\n");
    return 0;
}

int device_release(struct inode *inode, struct file* filep)
{
    printk("PimBT is closed! \n");
    return 0;
}

int device_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size, pgd, baseaddr, i;
	unsigned long physaddr, pfn;

	physaddr = (unsigned long)(BASE_PIM_DEV + 0x2f000000);
	pfn = physaddr >> PAGE_SHIFT;

	printk("Device mmap %lx %lx\n", physaddr, pfn);

	size = vma->vm_end - vma->vm_start;

	if (size > SIZE_PIM_DEV)
		size = SIZE_PIM_DEV;

	vma->vm_flags |= VM_SHARED;
	vma->vm_page_prot = __pgprot_modify(vma->vm_page_prot, PTE_ATTRINDX_MASK, 
										PTE_ATTRINDX(MT_DEVICE_nGnRE) | PTE_PXN | PTE_UXN);
	//pgprot_noncached(vma->vm_page_prot);

	remap_pfn_range(vma,
					vma->vm_start,
					pfn,
					size,
					vma->vm_page_prot);

	// Write the base of page table to hardware
	pgd = virt_to_phys(current->mm->pgd);

	for (i = 0; i < 4; i++) {
		baseaddr = BASE_PIM_DEV + (i << 8);

		pgd = virt_to_phys(current->mm->pgd);

		outl((unsigned int)pgd, baseaddr + PageTableLo);
		outl((unsigned int)(pgd >> 32), baseaddr + PageTableHi);
	}

	return 0;
}


ssize_t device_read(struct file* filp, char* buf, size_t count, loff_t* f_pos)
{
    printk("PimBT is being read\n");
    memset(buf,'a',count);
        
    //printk("count: %d\n",count);
    return count;
}

ssize_t device_write(struct file* filp, const char __user * buf, size_t count, loff_t* f_pos)
{
    int i, pagenum;
    int *root = (int *)(*(long *)buf);
	unsigned long pa, pgd;
	struct page *phy_page;
	struct vm_area_struct *vma;

	printk("count = %ld, root = (va) %lx, key = %lx\n", 
		   count, (unsigned long)root, *((long *)buf +1 ));
    for (i = 0; i < 8 ; i++)
		printk("data = %d\n", root[i]);
	
	pagenum = get_user_pages(current, current->mm, (unsigned long)root, 1, 0, 0, &phy_page, &vma);

	if (pagenum == 0)
		{
			printk("Unable to lock the user page\n");
		}
	else 
		{
			pa = page_to_phys(phy_page);
			printk("Physical address is %lx, vma start = %lx, vma end = %lx\n", pa, vma->vm_start, vma->vm_end);
		}

	pgd = virt_to_phys(current->mm->pgd);

	printk("pgd address is (va) %lx, (pa) %lx\n", (unsigned long)current->mm->pgd, pgd);

    outl((unsigned int)(unsigned long)root, BASE_PIM_DEV);
    outl((unsigned int)((unsigned long)root >> 32), BASE_PIM_DEV + 4);
    outl((unsigned int)pgd, BASE_PIM_DEV + 8);
    outl((unsigned int)((unsigned long)pgd >> 32), BASE_PIM_DEV + 12);

    return count;
}

void convert_to_pa(unsigned long va)
{

	int pagenum;
	unsigned long pa, pgd;
	struct page *phy_page;
	struct vm_area_struct *vma;

	printk("root = (va) %lx\n", va);
	
	pagenum = get_user_pages(current, current->mm, va, 1, 0, 0, &phy_page, &vma);

	if (pagenum == 0) {
		printk("Unable to lock the user page\n");
	}
	else {
		pa = page_to_phys(phy_page);
		printk("Physical address is %lx, vma start = %lx, vma end = %lx\n", 
			   pa, vma->vm_start, vma->vm_end);
	}

	pgd = virt_to_phys(current->mm->pgd);

	printk("pgd address is (va) %lx, (pa) %lx\n", (unsigned long)current->mm->pgd, pgd);

}

long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	unsigned long *buf;
	unsigned long tempbuf[3];
	unsigned long baseaddr = 0;
	int retval = -1;
	unsigned long pgd;
	int done;
	//	unsigned long lo, hi;

	//	printk("ioctl_num %ld, %ld, %ld\n", ioctl_num, IOCTL_START_TRAVERSE, IOCTL_POLL_DONE);

	switch (ioctl_num) {
	case IOCTL_START_TRAVERSE:
		buf = (unsigned long *)ioctl_param;

		if (0 != copy_from_user(tempbuf, buf, sizeof(unsigned long) * 3)) {
			printk("Unable to get buf %lx from user", (unsigned long) buf);
		}

		baseaddr = BASE_PIM_DEV + (buf[2] << 8);

		pgd = virt_to_phys(current->mm->pgd);

		outl((unsigned int)buf[0], baseaddr + RootAddrLo);
		outl((unsigned int)(buf[0] >> 32), baseaddr + RootAddrHi);

		outl((unsigned int)pgd, baseaddr + PageTableLo);
		outl((unsigned int)(pgd >> 32), baseaddr + PageTableHi);

		outl((unsigned int)buf[1], baseaddr + KeyLo);
		outl((unsigned int)(buf[1] >> 32), baseaddr + KeyHi);

		outl(1, baseaddr + StartTrav);

		retval = 0;

		break;

	case IOCTL_POLL_DONE:

		buf = (unsigned long *)ioctl_param;

		get_user(baseaddr, buf);

		convert_to_pa(baseaddr);

		done = 1;
		
		retval = done;

		break;
	}

	return retval;
}

module_init(device_init);
module_exit(device_exit);


