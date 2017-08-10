#include "mmkeg.h"
#include "dlist.h"
#include "misc.h"
#include "rbtree.h"

#define SLB_LABEL (0xeded00122100dedeUL)
#define KEG_LABEL (0xdede00322300ededUL)

#define SLB_OBJ_LABEL (0xabcd7890)

#define UFB_USED (1)

struct _slb_obj_header
{
	unsigned int _obj_label;
	unsigned int _obj_flag;
}__attribute__((aligned(8)));

struct _slb_node
{
	struct dlnode _fln;
	struct _slb_obj_header* _obj;
}__attribute__((aligned(8)));

struct _mmslab_impl
{
	unsigned long _chunk_label;
	struct mmslab _the_slb;
	struct dlist _free_list;
	struct rbnode _rb_node;

	void* _chunk_addr;

	unsigned long _actual_obj_size;
	unsigned long _obj_count;

	struct _slb_node* _node_pool;
}__attribute__((aligned(8)));


static long _slb_destroy(struct _mmslab_impl* umi);


static inline void* _get_payload(struct _slb_obj_header* uoh)
{
	return (void*)(uoh + 1);
}

static inline void _set_flag(struct _slb_obj_header* uoh, unsigned int flag)
{
	uoh->_obj_flag |= flag;
}

static inline void _clear_flag(struct _slb_obj_header* uoh, unsigned int flag)
{
	uoh->_obj_flag &= ~flag;
}

static inline struct _mmslab_impl* _conv_slab(struct mmslab* mm)
{
	return (struct _mmslab_impl*)((void*)mm - (unsigned long)(&((struct _mmslab_impl*)(0))->_the_slb));
}

static inline struct _mmslab_impl* _conv_slab_by_rbnode(struct rbnode* rbn)
{
	return (struct _mmslab_impl*)((void*)rbn - (unsigned long)(&((struct _mmslab_impl*)(0))->_rb_node));
}

static inline struct _slb_node* _conv_fln(struct dlnode* fln)
{
	return (struct _slb_node*)((void*)fln - (unsigned long)(&((struct _slb_node*)(0))->_fln));
}

static inline struct _slb_node* _get_node_from_obj(struct _mmslab_impl* umi, void* uoh)
{
	unsigned long idx;

	if(uoh < umi->_chunk_addr || uoh >= umi->_the_slb.addr_end)
		goto error_ret;

	idx = (uoh - umi->_chunk_addr) / umi->_actual_obj_size;
	if(idx >= umi->_obj_count)
		goto error_ret;

	return &umi->_node_pool[idx];
error_ret:
	return 0;
}

static inline struct _slb_node* _fetch_fln(struct _mmslab_impl* umi)
{
	struct dlnode* dln = lst_pop_front(&umi->_free_list);
	if(!dln) goto error_ret;

	lst_clr(dln);

	return _conv_fln(dln);
error_ret:
	return 0;
}

static inline void _return_fln(struct _mmslab_impl* umi, struct _slb_node* un)
{
	lst_clr(&un->_fln);
	lst_push_front(&umi->_free_list, &un->_fln);
}

static struct _mmslab_impl* _slb_create(void* addr, unsigned long size, unsigned long obj_size)
{
	struct _mmslab_impl* umi;
	void* cur_pos = addr;
	unsigned long chunk_size;

	if(!addr || ((unsigned long)addr & 63) != 0) goto error_ret;
	if(size <= sizeof(struct _mmslab_impl))
		goto error_ret;

	umi = (struct _mmslab_impl*)addr;
	if(umi->_chunk_label == SLB_LABEL)
		goto error_ret;

	cur_pos = move_ptr_align64(cur_pos, sizeof(struct _mmslab_impl));

	umi->_the_slb.addr_begin = addr;
	umi->_the_slb.addr_end = addr + size;

	umi->_the_slb.obj_size = obj_size;
	umi->_actual_obj_size = align8(obj_size + sizeof(struct _slb_obj_header));

	umi->_obj_count = (umi->_the_slb.addr_end - cur_pos) / (sizeof(struct _slb_node) + umi->_actual_obj_size);

	umi->_node_pool = (struct _slb_node*)cur_pos;
	cur_pos = move_ptr_align64(cur_pos, sizeof(struct _slb_node) * umi->_obj_count);

	umi->_chunk_addr = cur_pos;

	lst_new(&umi->_free_list);

	for(unsigned long i = 0; i < umi->_obj_count; ++i)
	{
		struct _slb_obj_header* uoh = (struct _slb_obj_header*)(umi->_chunk_addr + i * umi->_actual_obj_size);
		uoh->_obj_label = SLB_OBJ_LABEL;
		uoh->_obj_flag = 0;

		umi->_node_pool[i]._obj = uoh;

		lst_clr(&umi->_node_pool[i]._fln);
		lst_push_back(&umi->_free_list, &umi->_node_pool[i]._fln);
	}

	rb_fillnew(&umi->_rb_node);
	umi->_rb_node.key = (void*)umi->_actual_obj_size;

	return umi;
error_ret:
	if(umi)
		_slb_destroy(umi);
	return 0;
}

struct _mmslab_impl* _slb_load(void* addr)
{
	struct _mmslab_impl* umi;
	void* cur_pos = addr;

	if(!addr || ((unsigned long)addr & 63) != 0) goto error_ret;

	umi = (struct _mmslab_impl*)(cur_pos);
	cur_pos = move_ptr_align64(addr, sizeof(struct _mmslab_impl));

	if(umi->_chunk_label != SLB_LABEL)
		goto error_ret;

	if(umi->_the_slb.addr_begin != addr || umi->_the_slb.addr_begin >= umi->_the_slb.addr_end)
		goto error_ret;

	return umi;
error_ret:
	return 0;
}

static long _slb_destroy(struct _mmslab_impl* umi)
{
	if(!umi) goto error_ret;

	if(umi->_chunk_label != SLB_LABEL)
		goto error_ret;

	umi->_chunk_label = 0;
	umi->_the_slb.addr_begin = 0;
	umi->_the_slb.addr_end = 0;

	return 0;
error_ret:
	return -1;
}

void* _slb_alloc(struct _mmslab_impl* umi)
{
	struct _slb_node* un;
	struct _slb_obj_header* uoh;
	if(!umi) goto error_ret;

	un = _fetch_fln(umi);
	if(!un) goto error_ret;

	uoh = un->_obj;
	if(!uoh || uoh->_obj_label != SLB_OBJ_LABEL || (uoh->_obj_flag & UFB_USED) != 0)
		goto error_ret;

	_set_flag(uoh, UFB_USED);

	return _get_payload(uoh);
error_ret:
	return 0;
}

long _slb_free(struct _mmslab_impl* umi, void* p)
{
	struct _slb_node* un;
	struct _slb_obj_header* uoh;
	if(!umi) goto error_ret;

	uoh = (struct _slb_obj_header*)(p - sizeof(struct _slb_obj_header));

	if(uoh->_obj_label != SLB_OBJ_LABEL || (uoh->_obj_flag & UFB_USED) == 0)
		goto error_ret;

	un = _get_node_from_obj(umi, uoh);
	if(!un || un->_obj != uoh)
		goto error_ret;

	_clear_flag(uoh, UFB_USED);

	_return_fln(umi, un);

	return 0;
error_ret:
	return -1;
}

long slb_check(struct _mmslab_impl* umi)
{
	for(long i = 0; i < umi->_obj_count; ++i)
	{
		struct _slb_node* node = &umi->_node_pool[i];

		if(node->_obj->_obj_label != SLB_OBJ_LABEL)
			goto error_ret;

		if((node->_obj->_obj_flag & UFB_USED) != 0)
			goto error_ret;
	}

	return 0;
error_ret:
	return -1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//

struct _mmkeg_impl
{
	unsigned long _chunk_label;
	struct mmkeg _the_keg;

	struct rbtree _slab_tree;
	void* _free_chunk;

}__attribute__((aligned(8)));


static inline struct _mmkeg_impl* _conv_keg(struct mmkeg* mm)
{
	return (struct _mmkeg_impl*)((void*)mm - (unsigned long)(&((struct _mmkeg_impl*)(0))->_the_keg));
}

static inline long _comp_slab(void* key, struct rbnode* node)
{
	struct _mmslab_impl* msi = _conv_slab_by_rbnode(node);

	if((unsigned long)key < msi->_actual_obj_size)
		return -1;
	else if((unsigned long)key > msi->_actual_obj_size)
		return 1;

	return 0;
}

static struct _mmslab_impl* _find_slab(struct _mmkeg_impl* mki, unsigned long obj_size)
{
	struct rbnode* hot;
	struct rbnode* rbn = rb_search(&mki->_slab_tree, (void*)obj_size, &hot);
	if(!rbn) goto error_ret;

	return _conv_slab_by_rbnode(rbn);
error_ret:
	return 0;
}

struct mmkeg* keg_create(void* addr, unsigned long size)
{
	struct _mmkeg_impl* mki;

	if(!addr || ((unsigned long)addr & 0x3F) != 0)
		goto error_ret;

	if(size <= sizeof(struct _mmkeg_impl))
		goto error_ret;

	mki = (struct _mmkeg_impl*)addr;
	if(mki->_chunk_label == KEG_LABEL)
		goto error_ret;

	mki->_chunk_label = KEG_LABEL;
	mki->_the_keg.addr_begin = addr;
	mki->_the_keg.addr_end = addr + size;

	rb_init(&mki->_slab_tree, _comp_slab);
	mki->_free_chunk = move_ptr_align64(addr, sizeof(struct _mmkeg_impl));
	mki->_the_keg.remain_size = mki->_the_keg.addr_end - mki->_free_chunk;

	return &mki->_the_keg;
error_ret:
	return 0;
}

struct mmkeg* keg_load(void* addr)
{
	struct _mmkeg_impl* mki;

	if(!addr || ((unsigned long)addr & 0x3F) != 0)
		goto error_ret;

	mki = (struct _mmkeg_impl*)addr;
	if(mki->_chunk_label != KEG_LABEL)
		goto error_ret;

	if(mki->_the_keg.addr_begin != addr || mki->_the_keg.addr_end <= addr + sizeof(struct _mmkeg_impl))
		goto error_ret;

	return &mki->_the_keg;
error_ret:
	return 0;
}

long keg_destroy(struct mmkeg* keg)
{
	struct _mmkeg_impl* mki = _conv_keg(keg);
	if(mki->_chunk_label != KEG_LABEL)
		goto error_ret;

	mki->_chunk_label = 0;
	mki->_the_keg.remain_size = 0;
	mki->_the_keg.addr_begin = 0;
	mki->_the_keg.addr_end = 0;

	return 0;
error_ret:
	return -1;
}

void* keg_alloc(struct mmkeg* keg, unsigned long size)
{
	struct _mmkeg_impl* mki = _conv_keg(keg);

error_ret:
	return 0;
}
long keg_free(struct mmkeg* keg, void* p)
{
	struct _mmkeg_impl* mki = _conv_keg(keg);

	return 0;
error_ret:
	return -1;
}

long keg_add_slab(struct mmkeg* keg, unsigned long size, unsigned long obj_size)
{
	long rslt;
	void* slab_end;
	struct _mmslab_impl* msi;

	struct _mmkeg_impl* mki = _conv_keg(keg);

	if(((unsigned long)mki->_free_chunk & 0x3F) != 0)
		goto error_ret;

	slab_end = mki->_free_chunk + size;
	if(slab_end > mki->_the_keg.addr_end)
		goto error_ret;

	msi = _slb_create(mki->_free_chunk, size, obj_size);
	if(!msi) goto error_ret;

	rslt = rb_insert(&mki->_slab_tree, &msi->_rb_node);
	if(!rslt) goto error_ret;

	return 0;
error_ret:
	return -1;
}


