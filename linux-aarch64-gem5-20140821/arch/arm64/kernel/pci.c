/*
 *  arm64 PCI host controller interface
 *
 *  Heavily based on bios32.c from arch/arm/
 */
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/of_pci.h>
#include <linux/slab.h>

static void pcibios_fixup_device_resources(struct pci_dev *dev)
{
	struct resource *bar_r;
	int bar;

	if (pci_has_flag(PCI_PROBE_ONLY)) {
		/*
		* If the firmware did not assign the BAR, zero out the
		* resource so the kernel doesn't attempt to assign
		* it later on in pci_assign_unassigned_resources
		*/
		for (bar = 0; bar <= PCI_STD_RESOURCE_END; bar++) {
			bar_r = &dev->resource[bar];
			if (bar_r->start == 0 && bar_r->end != 0) {
				bar_r->flags = 0;
				bar_r->end = 0;
			}
		}
	}
}

void pcibios_fixup_bus(struct pci_bus *bus)
{
	struct pci_dev *dev;

	pci_read_bridge_bases(bus);
	list_for_each_entry(dev, &bus->devices, bus_list)
		pcibios_fixup_device_resources(dev);
}

static void pcibios_init_hw(struct device *parent, struct hw_pci *hw,
			    struct list_head *head)
{
	struct pci_sys_data *sys = NULL;
	int ret;
	int nr, busnr;

	for (nr = busnr = 0; nr < hw->nr_controllers; nr++) {
		sys = kzalloc(sizeof(struct pci_sys_data), GFP_KERNEL);
		if (!sys)
			panic("PCI: unable to allocate sys data!");

		sys->domain  = hw->domain;
		sys->busnr   = busnr;
		sys->add_bus = hw->add_bus;
		sys->remove_bus = hw->remove_bus;
		INIT_LIST_HEAD(&sys->resources);

		if (hw->private_data)
			sys->private_data = hw->private_data[nr];

		ret = hw->setup(nr, sys);

		if (ret > 0) {
			if (list_empty(&sys->resources)) {
				pr_err("PCI: no resources found\n");
				ret = -ENODEV;
				kfree(sys);
				break;
			}

			sys->bus = pci_scan_root_bus(parent, sys->busnr,
					hw->ops, sys, &sys->resources);
			if (!sys->bus)
				panic("PCI: unable to scan bus!");

			busnr = sys->bus->busn_res.end + 1;

			list_add(&sys->node, head);
		} else {
			kfree(sys);
			if (ret < 0)
				break;
		}
	}
}

void pci_common_init_dev(struct device *parent, struct hw_pci *hw)
{
	struct pci_sys_data *sys;
	LIST_HEAD(head);

	pci_add_flags(PCI_REASSIGN_ALL_RSRC);
	pcibios_init_hw(parent, hw, &head);
	pci_fixup_irqs(pci_common_swizzle, of_irq_parse_and_map_pci);

	list_for_each_entry(sys, &head, node) {
		struct pci_bus *bus = sys->bus;

		if (!pci_has_flag(PCI_PROBE_ONLY)) {
			/*
			 * Size the bridge windows.
			 */
			pci_bus_size_bridges(bus);

			/*
			 * Assign resources.
			 */
			pci_bus_assign_resources(bus);
		}

		/*
		 * Tell drivers about devices found.
		 */
		pci_bus_add_devices(bus);
	}
}

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				resource_size_t size, resource_size_t align)
{
	return ALIGN(res->start, align);
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	if (pci_has_flag(PCI_PROBE_ONLY))
		return 0;

	return pci_enable_resources(dev, mask);
}

int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	struct pci_sys_data *root = dev->sysdata;
	unsigned long phys;

	if (mmap_state == pci_mmap_io) {
		return -EINVAL;
	} else {
		phys = vma->vm_pgoff + (root->mem_offset >> PAGE_SHIFT);
	}

	/*
	 * Mark this as IO
	 */
	vma->vm_page_prot = write_combine ?
			    pgprot_writecombine(vma->vm_page_prot) :
			    pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, phys,
			     vma->vm_end - vma->vm_start,
			     vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

void pcibios_add_bus(struct pci_bus *bus)
{
	struct pci_sys_data *sys = bus->sysdata;
	if (sys->add_bus)
		sys->add_bus(bus);
}

void pcibios_remove_bus(struct pci_bus *bus)
{
	struct pci_sys_data *sys = bus->sysdata;
	if (sys->remove_bus)
		sys->remove_bus(bus);
}
