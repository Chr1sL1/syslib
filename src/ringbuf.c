#include "ringbuf.h"
#include "misc.h"
#include "mmspace.h"
#include <string.h>

#define RING_BUF_TAG		(0x1212dbdb1212dbdb)

struct _ring_buf_impl
{
	unsigned long _rb_tag;
	struct ring_buf _the_buf;

	volatile long _r_offset;
	volatile long _w_offset;
};

static struct mmzone* __the_ring_buf_zone = 0;

static inline struct _ring_buf_impl* _conv_rb(struct ring_buf* rb)
{
	return (struct _ring_buf_impl*)((unsigned long)rb - (unsigned long)&(((struct _ring_buf_impl*)(0))->_the_buf));
}

struct ring_buf* rbuf_create(void* addr, long size)
{
	struct _ring_buf_impl* rbi;

	if(!addr || size <= sizeof(struct _ring_buf_impl)) goto error_ret;
	if(((unsigned long)addr & 0x7) != 0) goto error_ret;
	if((size & 0x7) != 0) goto error_ret;

	if(!__the_ring_buf_zone)
		__the_ring_buf_zone = mm_zcreate("sys_ringbuf", sizeof(struct _ring_buf_impl));
	if(!__the_ring_buf_zone) goto error_ret;

	rbi = mm_zalloc(__the_ring_buf_zone);
	if(!rbi) goto error_ret;

	rbi->_rb_tag = RING_BUF_TAG;
	rbi->_r_offset = 0;
	rbi->_w_offset = 0;

	rbi->_the_buf.addr_begin = addr;
	rbi->_the_buf.size = size;

	return &rbi->_the_buf;
error_ret:
	return 0;
}

long rbuf_destroy(struct ring_buf* rbuf)
{
	struct _ring_buf_impl* rbi;
	if(!rbuf) goto error_ret;

	rbi = _conv_rb(rbuf);
	if(rbi->_rb_tag != RING_BUF_TAG)
		goto error_ret;

	return mm_zfree(__the_ring_buf_zone, rbi);
error_ret:
	return -1;
}

long rbuf_write_block(struct ring_buf* rbuf, const void* data, long datalen)
{
	long r_offset, w_offset;
	long remain;

	struct _ring_buf_impl* rbi = _conv_rb(rbuf);
	if(rbi == 0 || rbi->_the_buf.addr_begin == 0) goto error_ret;

	r_offset = rbi->_r_offset;
	w_offset = rbi->_w_offset;

	if(w_offset < r_offset)
	{
		remain = r_offset - w_offset;
		if(remain < datalen) goto error_ret;

		memcpy(rbi->_the_buf.addr_begin + w_offset, data, datalen);
		w_offset += datalen;

		goto succ_ret;
	}
	else if(w_offset + datalen < r_offset + rbi->_the_buf.size)
	{
		remain = rbi->_the_buf.size - w_offset;
		if(remain >= datalen)
		{
			memcpy(rbi->_the_buf.addr_begin + w_offset, data, datalen);
			w_offset += datalen;
		}
		else
		{
			long remain2 = datalen - remain;
			memcpy(rbi->_the_buf.addr_begin + w_offset, data, remain);
			memcpy(rbi->_the_buf.addr_begin, data + remain, remain2);
			w_offset = remain2;
		}

		goto succ_ret;
	}

	goto error_ret;

succ_ret:
	rbi->_w_offset = w_offset;
	return 0;
error_ret:
	return -1;
}

long rbuf_read_block(struct ring_buf* rbuf, void* buf, long readlen)
{
	long r_offset, w_offset;
	long remain;

	struct _ring_buf_impl* rbi = _conv_rb(rbuf);
	if(rbi == 0 || rbi->_the_buf.addr_begin == 0) goto error_ret;

	r_offset = rbi->_r_offset;
	w_offset = rbi->_w_offset;

	if(r_offset <= w_offset)
	{
		remain = w_offset - r_offset;
		if(remain < readlen) goto error_ret;

		memcpy(buf, rbi->_the_buf.addr_begin + r_offset, readlen);
		r_offset += readlen;

		goto succ_ret;
	}
	else if(r_offset + readlen < w_offset + rbi->_the_buf.size)
	{
		remain = rbi->_the_buf.size - r_offset;
		if(remain >= readlen)
		{
			memcpy(buf, rbi->_the_buf.addr_begin + r_offset, readlen);
			r_offset += readlen;
		}
		else
		{
			long remain2 = readlen - remain;
			memcpy(buf, rbi->_the_buf.addr_begin + r_offset, remain);
			memcpy(buf + remain, rbi->_the_buf.addr_begin, remain2);
			r_offset = remain2;
		}
		goto succ_ret;
	}

	goto error_ret;

succ_ret:
	rbi->_r_offset = r_offset;
	return 0;
error_ret:
	return -1;
}

