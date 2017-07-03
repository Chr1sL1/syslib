#include "ringbuf.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>

#define RING_BUF_TAG		(0x1212dbdb)
#define RING_BUF_MAX_REF	(2)

#pragma pack(1)
struct _ring_buf_head
{
	volatile unsigned int _rb_tag;
	volatile int _ref_count;
	volatile long _size;
	volatile long _r_offset;
	volatile long _w_offset;
};
#pragma pack()

struct _ring_buf_impl
{
	struct ring_buf _the_buf;

	void* _chunk_addr;
	long _chunk_size;

	struct _ring_buf_head* _hd;
};

static inline struct _ring_buf_impl* _conv_rb(struct ring_buf* rb)
{
	return (struct _ring_buf_impl*)((unsigned long)rb - (unsigned long)&(((struct _ring_buf_impl*)(0))->_the_buf));
}

long rbuf_new(void* addr, long size)
{
	struct _ring_buf_head* hd;

	if(addr == 0 || size <= sizeof(struct _ring_buf_head))
		goto error_ret;

	if(((unsigned long)addr & 0x7) != 0 || (size & 0x7) != 0)
		goto error_ret;

	hd = (struct _ring_buf_head*)addr;
	if(hd->_rb_tag == RING_BUF_TAG || hd->_ref_count > 0)
		goto error_ret;

	hd->_rb_tag = RING_BUF_TAG;
	hd->_size = size - sizeof(struct _ring_buf_head);
	hd->_r_offset = 0;
	hd->_w_offset = 0;
	hd->_ref_count = 0;

	return 0;
error_ret:
	return -1;
}

struct ring_buf* rbuf_open(void* addr)
{
	struct _ring_buf_impl* rbi = 0;
	struct _ring_buf_head* hd;

	if(addr == 0)
		goto error_ret;

	hd = (struct _ring_buf_head*)addr;
	if(hd->_rb_tag != RING_BUF_TAG)
		goto error_ret;

	rbi = (struct _ring_buf_impl*)malloc(sizeof(struct _ring_buf_impl));
	if(rbi == 0)
		goto error_ret;

	rbi->_hd = hd;
	rbi->_the_buf.addr = addr;
	rbi->_the_buf.size = hd->_size + sizeof(struct _ring_buf_head);
	rbi->_chunk_addr = (void*)(hd + 1);
	rbi->_chunk_size = hd->_size;

	++hd->_ref_count;

	return &rbi->_the_buf;
error_ret:
	if(rbi)
		free(rbi);
	return 0;

}

long rbuf_close(struct ring_buf** rbuf)
{
	struct _ring_buf_head* hd;
	struct _ring_buf_impl* rbi = _conv_rb(*rbuf);
	if(rbi == 0)
		goto error_ret;

	hd = (struct _ring_buf_head*)rbi->_the_buf.addr;
	if(hd->_rb_tag != RING_BUF_TAG || hd->_ref_count <= 0)
		goto error_ret;

	--hd->_ref_count;

	free(rbi);
	*rbuf = 0;

	return 0;
error_ret:
	if(rbi)
		free(rbi);
	*rbuf = 0;
	return -1;

}

long rbuf_del(struct ring_buf** rbuf)
{
	struct _ring_buf_head* hd;
	struct _ring_buf_impl* rbi = _conv_rb(*rbuf);
	if(rbi == 0) goto error_ret;

	hd = (struct _ring_buf_head*)rbi->_the_buf.addr;
	if(hd->_rb_tag != RING_BUF_TAG)
		goto error_ret;

	hd->_rb_tag = 0;

	free(rbi);
	*rbuf = 0;

	return 0;
error_ret:
	if(rbi)
		free(rbi);
	*rbuf = 0;
	return -1;
}

long rbuf_write_block(struct ring_buf* rbuf, const void* data, long datalen)
{
	long r_offset, w_offset;
	long remain;

	struct _ring_buf_impl* rbi = _conv_rb(rbuf);
	if(rbi == 0 || rbi->_chunk_addr == 0) goto error_ret;

	r_offset = rbi->_hd->_r_offset;
	w_offset = rbi->_hd->_w_offset;

	if(w_offset < r_offset)
	{
		remain = r_offset - w_offset;
		if(remain < datalen) goto error_ret;

		memcpy(rbi->_chunk_addr + w_offset, data, datalen);
		w_offset += datalen;

		goto succ_ret;
	}
	else if(w_offset + datalen < r_offset + rbi->_chunk_size)
	{
		remain = rbi->_chunk_size - w_offset;
		if(remain >= datalen)
		{
			memcpy(rbi->_chunk_addr + w_offset, data, datalen);
			w_offset += datalen;
		}
		else
		{
			long remain2 = datalen - remain;
			memcpy(rbi->_chunk_addr + w_offset, data, remain);
			memcpy(rbi->_chunk_addr, data + remain, remain2);
			w_offset = remain2;
		}

		goto succ_ret;
	}

	goto error_ret;

succ_ret:
	rbi->_hd->_w_offset = w_offset;
	return 0;
error_ret:
	return -1;
}

long rbuf_read_block(struct ring_buf* rbuf, void* buf, long readlen)
{
	long r_offset, w_offset;
	long remain;

	struct _ring_buf_impl* rbi = _conv_rb(rbuf);
	if(rbi == 0 || rbi->_chunk_addr == 0) goto error_ret;

	r_offset = rbi->_hd->_r_offset;
	w_offset = rbi->_hd->_w_offset;

	if(r_offset <= w_offset)
	{
		remain = w_offset - r_offset;
		if(remain < readlen) goto error_ret;

		memcpy(buf, rbi->_chunk_addr + r_offset, readlen);
		r_offset += readlen;

		goto succ_ret;
	}
	else if(r_offset + readlen < w_offset + rbi->_chunk_size)
	{
		remain = rbi->_chunk_size - r_offset;
		if(remain >= readlen)
		{
			memcpy(buf, rbi->_chunk_addr + r_offset, readlen);
			r_offset += readlen;
		}
		else
		{
			long remain2 = readlen - remain;
			memcpy(buf, rbi->_chunk_addr + r_offset, remain);
			memcpy(buf + remain, rbi->_chunk_addr, remain2);
			r_offset = remain2;
		}
		goto succ_ret;
	}

	goto error_ret;

succ_ret:
	rbi->_hd->_r_offset = r_offset;
	return 0;
error_ret:
	return -1;
}

