#include "dev/arm/amba_device.hh"
#include "params/PimDevice.hh"
#include "sim/system.hh"
#include "sim/serialize.hh"
#include "base/statistics.hh"

#define PIM_DEVICE_CH            (4)
#define BTREE_ORDER              (16)
#define FIRST_LEVEL_PTABLE_PTRS  (512)
#define SECOND_LEVEL_PTABLE_PTRS (512)

class PimDevice : public AmbaDmaDevice
{
	/*
	  protected:
	  class PimPort : public MasterPort
	  {
	  public:

	  PimDevice* device;
	  System* sys;
	  bool recvTimingResp(PacketPtr pkt);
	  void traverse(Addr phys);
	  PimPort(PimDevice *dev, System *s);
	  const MasterID masterId;
	  void recvRetry() ;
	  };
	*/

private:
	
    /*
	typedef struct bt_node {
		// TODO bad hack!
		void *pointers[BTREE_ORDER]; // for non-leaf nodes, point to bt_nodes
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
	*/
	typedef struct bt_node {
		struct bt_node *next;
		unsigned long pad[7];
						  //int count;
	} bt_node;



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

	/** Channel mask **/
	static const int ChannelShft       = 8;
	static const int ChannelMask       = ((1 << 8) - 1);

	/* Const related to DMA */
	// max number of DMAs
    static const int maxOutstandingDma = 16;
	// max size of each DMA
    static const int dmaSize           = 64;  	//Each DMA get a cache line back (64B)
	static const int pageWalkDmaSize   = 8;

	/** Const for event trigger */
	static const int PageWalkLevel0    = 0;
	static const int PageWalkLevel1    = 1;
	static const int PageWalkLevel2    = 2;
	static const int PageWalkLevel3    = 3;
	static const int FetchBtNode       = 4;
	static const int FetchPageTable    = 5;

	std::string devname;

	/* Buffer for registers */
	uint64_t rootAddr[PIM_DEVICE_CH];
	uint64_t pageTableAddr[PIM_DEVICE_CH];
	uint64_t key[PIM_DEVICE_CH];
	bool startTrav[PIM_DEVICE_CH];
	bool doneTrav[PIM_DEVICE_CH];
	uint64_t item[PIM_DEVICE_CH];
	uint32_t itemIndex[PIM_DEVICE_CH];
	uint64_t leafAddr[PIM_DEVICE_CH];
	bool isPageTableFetched;
	int64_t fetchPageTableIndex;
	
	/* Buffer for BT node */
	bt_node btNodeDmaBuffer[PIM_DEVICE_CH];

	/* Buffer for First Level Page Table */
	uint64_t firstLevelPageTable[FIRST_LEVEL_PTABLE_PTRS];
	uint64_t secondLevelPageTable[FIRST_LEVEL_PTABLE_PTRS][SECOND_LEVEL_PTABLE_PTRS];

	/* Variabled related to VA to PA translation, NOTE: we can only support single PTW at once */
	uint64_t virtualAddrToTrans[PIM_DEVICE_CH];                
	uint64_t physicalAddrTransalted[PIM_DEVICE_CH];
	uint64_t pageWalkDmaBuffer[PIM_DEVICE_CH];
	bool doingPageWalk[PIM_DEVICE_CH];
	bool btNodeCrossPage[PIM_DEVICE_CH];
	uint64_t btNodeCrossPageHeadSize[PIM_DEVICE_CH];

	/* Buffer for stats */
	Tick travRequestStart[PIM_DEVICE_CH];
	Tick vaToPaStart[PIM_DEVICE_CH];
	Tick btNodeFetchStart[PIM_DEVICE_CH];
	uint64_t totalTravRequest[PIM_DEVICE_CH];

public:

	typedef PimDeviceParams Params;

protected:

	class DmaDoneEvent : public Event
    {
	private:
        PimDevice &obj;
	    uint64_t eventTrigger;
		uint64_t eventChannel;

	public:
        DmaDoneEvent(PimDevice *_obj)
            : Event(), obj(*_obj) {}

        void process() {
			uint64_t trigger = this->eventTrigger;
			uint64_t ch = this->eventChannel;
            obj.dmaDoneEventFree.push_back(this);
            obj.dmaDone(trigger, ch);
        }

		void setEventTrigger(uint64_t trigger, uint64_t ch) {
			eventTrigger = trigger;
			eventChannel = ch;
		}

        const std::string name() const {
            return obj.name() + ".DmaDoneEvent";
        }
    };

	class PimTlb
	{
	private:
		bool valid;
		uint64_t va_start;
		uint64_t va_size;
		uint64_t pa;

	public:
		PimTlb()
			: valid(false) {}

		bool isValid() {
			return valid;
		}

		bool isHit(uint64_t va, uint64_t *ret_pa) {
			if (valid) {
				if (va >= va_start && va < va_start + va_size) {
					*ret_pa = (pa & ~(va_size - 1)) | (va & (va_size - 1));
					return true;
				}
			}
			return false;
		}

		void setEntry(uint64_t tlb_va, uint64_t tlb_va_size, uint64_t tlb_pa) {
			va_start = tlb_va;
			va_size = tlb_va_size;
			pa = tlb_pa;
			valid = true;
		}
	};

	// get a free DMA done event from the vector
	DmaDoneEvent* getFreeDmaDoneEvent();

	// when the DMA is done, this function will be triggered.
    void dmaDone(uint64_t eventTrigger, uint64_t ch);

	// deal with the DMA done event for page table walk
	void pageWalkDmaBack(uint64_t eventTrigger, uint64_t ch);

	// deal with the DMA done event for BT node fetch
	void fetchBtNodeBack(uint64_t ch);

	// When first level page walk done, invoke this function
	void firstLevelPageWalkDone(uint64_t pageTableEntry, uint64_t ch);

	// When second level page walk done, invoke this function
	void secondLevelPageWalkDone(uint64_t pageTableEntry, uint64_t ch);


	// when page table walk has been done, invoke this function
	void pageWalkDone(uint64_t physAddr, uint64_t ch);

	// the function to do VA to PA translation
	uint64_t transVirtAddrToPhysAddr(uint64_t virtAddr, uint64_t ch);

	// insert a TLB entry
	void insertTlbEntry(uint64_t virtAddr, uint64_t virtAddrSize, uint64_t physAddr);

	// start B-tree traversal
	void startBtreeTraverse(uint64_t ch);

	// start grab first level page table
	void startGrabPageTable();

	// grab second level page table
	void grabSecondPageTable();

	// whether the address of BT node exceeds a page
	bool isBtNodeCrossPage(unsigned long addr);

	/**@{*/
    /**
     * All pre-allocated DMA done events
     *
     * The PIM device model preallocates maxOutstandingDma number of
     * DmaDoneEvents to avoid having to heap allocate every single
     * event when it is needed. In order to keep track of which events
     * are in flight and which are ready to be used, we use two
     * different vectors. dmaDoneEventAll contains <i>all</i>
     * DmaDoneEvents that the object may use, while dmaDoneEventFree
     * contains a list of currently <i>unused</i> events. When an
     * event needs to be scheduled, the last element of the
     * dmaDoneEventFree is used and removed from the list. When an
     * event fires, it is added to the end of the
     * dmaEventFreeList. dmaDoneEventAll is never used except for in
     * initialization and serialization.
     */
	std::vector<DmaDoneEvent> dmaDoneEventAll;
    /** Unused DMA done events that are ready to be scheduled */
    std::vector<DmaDoneEvent *> dmaDoneEventFree;

	/**
	 * The vector for all TLB entries, it's not sorted based on LRU
	 */
	std::vector<PimTlb> tlbEntryAll;
    /** Unused DMA done events that are ready to be scheduled */
    std::list<PimTlb *> tlbEntryList;

	/** Stats */
    Stats::Scalar totalVaToPa;
	Stats::Scalar totalTlbHit;
	Stats::Formula tlbHitRate;

	Stats::Vector totalRequest;
	Stats::Vector totalLatency;
	Stats::Vector totalVaToPaLatency;
	Stats::Vector totalBtNodeLatency;
	Stats::Vector totalVaToPaNum;
	Stats::Vector totalBtNodeNum;
	Stats::Formula overallAvgLatency;
	Stats::Formula overallAvgVaToPaLatency;
	Stats::Formula overallAvgBtNodeLatency;
	Stats::Formula accAvgVaToPaLatency;
	Stats::Formula accAvgBtNodeLatency;

    static const uint64_t AMBA_ID       = ULL(0xb105f00d00142222);

	const Params * params() const
	{
		return dynamic_cast<const Params *>(_params);
	}

public:
	PimDevice(Params *p);
	~PimDevice();

	virtual Tick read(PacketPtr pkt);
	virtual Tick write(PacketPtr pkt);

    virtual void serialize(std::ostream &os);
    virtual void unserialize(Checkpoint *cp, const std::string &section);

	/**
     * Register local statistics.
     */
    void regStats();

    /**
     * Determine the address ranges that this device responds to.
     *
     * @return a list of non-overlapping address ranges
     */
    AddrRangeList getAddrRanges() const;

};


