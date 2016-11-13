/*
 *  asm/pci.h
 *  Heavily based on mach/pci.h from arch/arm/
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_PCI_H
#define __ASM_PCI_H

#include <linux/ioport.h>
#include <asm-generic/pci-dma-compat.h>
#include <asm-generic/pci-bridge.h>

struct pci_sys_data;
struct pci_ops;
struct pci_bus;
struct device;

struct hw_pci {
	int		domain;
	struct pci_ops	*ops;
	int		nr_controllers;
	void		**private_data;
	int		(*setup)(int nr, struct pci_sys_data *);
	void		(*add_bus)(struct pci_bus *bus);
	void		(*remove_bus)(struct pci_bus *bus);
};

/*
 * Per-controller structure
 */
struct pci_sys_data {
	int		domain;
	struct list_head node;
	int		busnr;		/* primary bus number			*/
	u64		mem_offset;	/* bus->cpu memory mapping offset	*/
	unsigned long	io_offset;	/* bus->cpu IO mapping offset		*/
	struct pci_bus	*bus;		/* PCI bus				*/
	struct list_head resources;	/* root bus resources (apertures)       */
	struct resource io_res;
	char		io_res_name[12];
	void		(*add_bus)(struct pci_bus *bus);
	void		(*remove_bus)(struct pci_bus *bus);
	void		*private_data;	/* platform controller private data	*/
};

/*
 * Call this with your hw_pci struct to initialise the PCI system.
 */
void pci_common_init_dev(struct device *, struct hw_pci *);

static inline int pcibios_assign_all_busses(void)
{
	return pci_has_flag(PCI_REASSIGN_ALL_RSRC);
}

static inline int pci_domain_nr(struct pci_bus *bus)
{
	struct pci_sys_data *root = bus->sysdata;

	return root->domain;
}

static inline int pci_proc_domain(struct pci_bus *bus)
{
	return pci_domain_nr(bus);
}

#define PCIBIOS_MIN_IO	(0x1000)
#define PCIBIOS_MIN_MEM	(0)

#define PCI_DMA_BUS_IS_PHYS     (0)

#define HAVE_PCI_MMAP
extern int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
                               enum pci_mmap_state mmap_state, int write_combine);

#endif /* __ASM_PCI_H */
