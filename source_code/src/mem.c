
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;
static pthread_mutex_t ram_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */
	//for (int i = 0; i < seg_table->size; i++) {
	//	// Enter your code here
	//	if (seg_table->table[i].v_index == index)
	//		return seg_table->table[i].pages;
	//}
	if (index < (addr_t)seg_table->size && seg_table->table[index].v_index == index) {
		return seg_table->table[index].pages;
	}
	return NULL;

}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL) {
		return 0;
	}

	//int i;
	//for (i = 0; i < page_table->size; i++) {
	//	if (page_table->table[i].v_index == second_lv) {
	//		/* TODO: Concatenate the offset of the virtual address
	//		 * to [p_index] field of page_table->table[i] to 
	//		 * produce the correct physical address and save it to
	//		 * [*physical_addr]  */
	//		*physical_addr = (page_table->table[i].p_index << OFFSET_LEN) | offset;
	//		return 1;
	//	}
	//}
	if (second_lv < (addr_t)page_table->size && page_table->table[second_lv].v_index == second_lv) {
		*physical_addr = page_table->table[second_lv].p_index << OFFSET_LEN | offset;
		return 1;
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = (size % PAGE_SIZE) ? size / PAGE_SIZE + 1 : size / PAGE_SIZE; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check (break pointer).
	 * */
	int phy_avail  = 0;
	int vir_avail  = 0;
	int free_frame = 0;

	// check physical space
	for (int i = 0; i < NUM_PAGES; ++i) {
		if (_mem_stat[i].proc == 0) ++free_frame;
		if (free_frame >= num_pages) {
			phy_avail = 1;
			break;
		}
	}

	// check virtual space
	if (proc->bp + size < (uint32_t)(1 << ADDRESS_SIZE))
		vir_avail = 1;

	mem_avail = phy_avail && vir_avail;
	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */

		int i = 0; int j = 0; int* prev = NULL;
		while (i < num_pages) {
			// for each new free frame found in the memory state structure
			if (_mem_stat[j].proc == 0) {
				// we first find target slot of the target page table
				addr_t page_address = ret_mem + i * PAGE_SIZE;
				addr_t first_lv = get_first_lv(page_address);
				struct page_table_t* target_page_table = get_page_table(first_lv, proc->seg_table);
				if (target_page_table == NULL) {	// if we're changing to a new slot of segment table
					proc->seg_table->table[first_lv].pages = 
						(struct page_table_t*)malloc(sizeof(struct page_table_t));	// allocate a new page table for this segment table slot
					proc->seg_table->table[first_lv].pages->size = 0;				// initialize its size
					proc->seg_table->table[first_lv].v_index = first_lv;			// and update its v_index value
					proc->seg_table->size = first_lv + 1;	// at this time the size of the segment table have been incremented
					target_page_table = proc->seg_table->table[first_lv].pages;		// further tasks' coming
				}
				// we've got the target page table, the new frame index will be updated to the target slot of this table
				addr_t second_lv = get_second_lv(page_address);
				target_page_table->table[second_lv].p_index = j;			// our main focus is here
				target_page_table->table[second_lv].v_index = second_lv;	// update v_index of this page table slot
				target_page_table->size = second_lv + 1;	// the page table size is also incremented

				// now its time to modify the memory state structure
				_mem_stat[j].proc  = proc->pid;	// register this frame is for this process
				_mem_stat[j].index = i;			// update the index field
				if (prev != NULL)	// if this is not the first frame in the allocated list
					*prev = j;		// then we update the next field of the preceding frame
				prev = &_mem_stat[j].next;	// and the next field of the current frame will in turn be updated 
				++i;	// now that another frame has been assigned to this process
				if (i == num_pages)	// if this is the last frame in the allocated list
					_mem_stat[j].next = -1;	// then the next field should be -1
			}
			++j;	// we continue to find another free frame
		}
	}
	pthread_mutex_unlock(&mem_lock);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */

	// first we check to see if the address send us to some invalid location of the segment table 
	addr_t first_lv = get_first_lv(address);
	struct page_table_t* target_table = get_page_table(first_lv, proc->seg_table);
	if (target_table == NULL) {	// if so then we terminate with error return value
		return 0;
	}
	// we do the same for the page table
	addr_t second_lv = get_second_lv(address);
	if (second_lv >= (addr_t)target_table->size || target_table->table[second_lv].v_index != second_lv)
		return 0;
	// okay this address is valid, let deallocate from the first frame of the allocated list
	addr_t proc_page_id = target_table->table[second_lv].p_index;
	int count = 0;	// let see how many frames were allocated starting at this address
	pthread_mutex_lock(&mem_lock);
	while (proc_page_id != -1) {	// while we not hitting the last frame yet
		_mem_stat[proc_page_id].proc = 0;	// then we reset the process id
		++count;
		proc_page_id = _mem_stat[proc_page_id].next;	// jump to next frame
	}
	pthread_mutex_unlock(&mem_lock);

	// finally we update the v_index fields of all the related pages to indicate they are no longer valid
	while (count > 0) {
		target_table->table[second_lv].v_index = -1;	// this going to be a really large number, ensuring no accidental matches
		++second_lv; --count;
		if (second_lv >= (addr_t)(1 << PAGE_LEN)) {		// if we're changing to a new segment table slot
			target_table = proc->seg_table->table[++first_lv].pages;
			second_lv = 0;
		}
	}
	return 1;
}

int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		return 0;
	}else{
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
					
			}
		}
	}
}


