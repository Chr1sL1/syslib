#ifndef __ipc_h__
#define __ipc_h__

struct ipc_peer
{
	long channel_id;
	long buffer_size;
};


struct ipc_peer* ipc_create(long channel_id, long buffer_size);
struct ipc_peer* ipc_link(long channel_id);

long ipc_unlink(struct ipc_peer* pr);
long ipc_destroy(struct ipc_peer* pr);


#endif

