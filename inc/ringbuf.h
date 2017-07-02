#ifndef __ring_buf_h__
#define __ring_buf_h__

struct ring_buf
{
	void* addr;
	long size;
};

long rbuf_new(void* addr, long size);
struct ring_buf* rbuf_open(void* addr);

long rbuf_close(struct ring_buf** rbuf);
long rbuf_del(struct ring_buf** rbuf);

long rbuf_write_block(struct ring_buf* rb, const void* data, long datalen);
long rbuf_read_block(struct ring_buf* rb, void* buf, long buflen);

//long rbuf_write_word(struct ring_buf* rb, unsigned short val);
//long rbuf_read_word(struct ring_buf* rb, unsigned short* val);
//
//long rbuf_write_dword(struct ring_buf* rb, unsigned int val);
//long rbuf_read_dword(struct ring_buf* rb, unsigned int* val);
//
//long rbuf_write_qword(struct ring_buf* rb, unsigned long val);
//long rbuf_read_qword(struct ring_buf* rb, unsigned long* val);


#endif //__ring_buf_h__

