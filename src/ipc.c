#include "ipc.h"
#include "shmem.h"
#include "ringbuf.h"
#include <stdlib.h>

struct _ipc_peer_impl
{
	struct ipc_peer _the_peer;
	struct shmm_blk* _shb;
	struct ring_buf* _rb;
};

static inline struct _ipc_peer_impl* _conv_peer(struct ipc_peer* pr)
{
	return (struct _ipc_peer_impl*)((unsigned long)pr - (unsigned long)&(((struct _ipc_peer_impl*)(0))->_the_peer));
}


struct ipc_peer* ipc_create(int channel_id, long buffer_size, long use_huge_tlb)
{
	long rslt;
	struct _ipc_peer_impl* ipi = 0;

	if(channel_id <= 0 || buffer_size <= 0)
		goto error_ret;

//	ipi = malloc(sizeof(struct _ipc_peer_impl));
//	if(!ipi) goto error_ret;
//
//	ipi->_shb = shmm_create(channel_id, 0, buffer_size, use_huge_tlb);
//	if(!ipi->_shb) goto error_ret;
//
//	rslt = rbuf_new(ipi->_shb->addr_begin, ipi->_shb->addr_end - ipi->_shb->addr_begin);
//	if(rslt < 0) goto error_ret;
//
//	ipi->_rb = rbuf_open(ipi->_shb->addr_begin);
//	if(!ipi->_rb) goto error_ret;
//
//	ipi->_the_peer.channel_id = channel_id;
//	ipi->_the_peer.buffer_size = buffer_size;

	return &ipi->_the_peer;
error_ret:
	if(ipi)
	{
//		if(ipi->_rb)
//		{
//			rbuf_close(&ipi->_rb);
//			rbuf_del(&ipi->_rb);
//		}
//		if(ipi->_shb)
//		{
//			shmm_close(ipi->_shb);
//			shmm_destroy(ipi->_shb);
//		}
//
//		free(ipi);
	}
	return 0;
}

struct ipc_peer* ipc_link(int channel_id)
{
	long rslt;
	struct _ipc_peer_impl* ipi = 0;

//	if(channel_id <= 0)
//		goto error_ret;
//
//	ipi = malloc(sizeof(struct _ipc_peer_impl));
//	if(!ipi) goto error_ret;
//
//	ipi->_shb = shmm_open(channel_id, 0);
//	if(!ipi->_shb) goto error_ret;
//
//	ipi->_rb = rbuf_open(ipi->_shb->addr_begin);
//	if(!ipi->_rb) goto error_ret;
//
//	ipi->_the_peer.channel_id = channel_id;
//	ipi->_the_peer.buffer_size = ipi->_rb->size;

	return &ipi->_the_peer;
error_ret:
	if(ipi)
	{
//		if(ipi->_rb)
//			rbuf_close(&ipi->_rb);
//		if(ipi->_shb)
//			shmm_close(ipi->_shb);

		free(ipi);
	}

	return 0;
}

long ipc_unlink(struct ipc_peer** pr)
{
	struct _ipc_peer_impl* ipi = _conv_peer(*pr);
//	if(!ipi) goto error_ret;

//	if(!ipi->_rb || !ipi->_shb)
//		goto error_ret;
//
//	rbuf_close(&ipi->_rb);
//	shmm_close(ipi->_shb);
//
//	free(*pr);
//	*pr = 0;

	return 0;
error_ret:
//	if(*pr)
//		free(*pr);
//	*pr = 0;
	return -1;
}


long ipc_destroy(struct ipc_peer** pr)
{
	struct _ipc_peer_impl* ipi = _conv_peer(*pr);
	if(!ipi) goto error_ret;

//	if(!ipi->_rb || !ipi->_shb)
//		goto error_ret;
//
//	rbuf_del(&ipi->_rb);
//	shmm_destroy(ipi->_shb);
//
//	free(*pr);
//	*pr = 0;

	return 0;
error_ret:
//	if(*pr)
//		free(*pr);
//	*pr = 0;
	return -1;
}

long ipc_write(struct ipc_peer* pr, const void* buff, long size)
{
	struct _ipc_peer_impl* ipi = _conv_peer(pr);
	if(!ipi) goto error_ret;

	if(!ipi->_rb || !ipi->_shb)
		goto error_ret;

	return rbuf_write_block(ipi->_rb, buff, size);
error_ret:
	return -1;
}

long ipc_read(struct ipc_peer* pr, void* buff, long size)
{
	struct _ipc_peer_impl* ipi = _conv_peer(pr);
	if(!ipi) goto error_ret;

	if(!ipi->_rb || !ipi->_shb)
		goto error_ret;

	return rbuf_read_block(ipi->_rb, buff, size);
error_ret:
	return -1;
}


