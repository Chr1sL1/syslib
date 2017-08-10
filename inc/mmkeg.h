#ifndef __mmkeg_h__
#define __mmkeg_h__

struct mmslab
{
	void* addr_begin;
	void* addr_end;
	unsigned long obj_size;
};

struct mmkeg
{
	void* addr_begin;
	void* addr_end;
	unsigned long remain_size;
};

//struct mmslab* slb_create(void* addr, unsigned long size, unsigned long obj_size);
//struct mmslab* slb_load(void* addr);
//
//long slb_destroy(struct mmslab* mm);
//
//void* slb_alloc(struct mmslab* mm);
//long slb_free(struct mmslab* mm, void* p);
//
//long slb_check(struct mmslab* mm);

struct mmkeg* keg_create(void* addr, unsigned long size);
struct mmkeg* keg_load(void* addr_begin);
long keg_destroy(struct mmkeg* keg);

void* keg_alloc(struct mmkeg* keg, unsigned long size);
long keg_free(struct mmkeg* keg, void* p);

long keg_add_slab(struct mmkeg* keg, unsigned long size, unsigned long obj_size);

#endif

