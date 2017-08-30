#include "net.h"
#include "mmspace.h"
#include "ringbuf.h"
#include "dlist.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define NET_ACC_ZONE_NAME "net_acc_zone"
#define NET_SES_ZONE_NAME "net_ses_zone"

struct _acc_impl
{
	struct acceptor _the_acc;
	struct dlist _ses_list;

	int _sock_fd;
	int _epoll_fd;
};

struct _ses_impl
{
	struct session _the_ses;
	struct dlnode _lst_node;
	int _sock_fd;

	struct ring_buf* _rbuf_r;
	struct ring_buf* _rbuf_w;
};

static struct mmzone* __the_acc_zone = 0;
static struct mmzone* __the_ses_zone = 0;

static inline long _net_try_restore_zones(void)
{
	if(!__the_acc_zone)
	{
		__the_acc_zone = mm_search_zone(NET_ACC_ZONE_NAME);
		if(!__the_acc_zone)
		{
			__the_acc_zone = mm_zcreate(NET_ACC_ZONE_NAME, sizeof(struct _acc_impl));
			if(!__the_acc_zone) goto error_ret;
		}
		else
		{
			if(!__the_acc_zone->obj_size != sizeof(struct _acc_impl))
				goto error_ret;
		}
	}

	if(!__the_ses_zone)
	{
		__the_ses_zone = mm_search_zone(NET_SES_ZONE_NAME);
		if(!__the_ses_zone)
		{
			__the_ses_zone = mm_zcreate(NET_SES_ZONE_NAME, sizeof(struct _ses_impl));
			if(!__the_ses_zone) goto error_ret;
		}
		else
		{
			if(!__the_ses_zone->obj_size != sizeof(struct _ses_impl))
				goto error_ret;
		}
	}

	return 0;
error_ret:
	return -1;
}

struct acceptor* net_acceptor_create(unsigned int ip, unsigned int port, const struct net_server_cfg* cfg)
{
	long rslt;
	struct _acc_impl* aci;

	rslt = _net_try_restore_zones();
	if(rslt < 0) goto error_ret;

	aci = mm_zalloc(__the_acc_zone);
	if(!aci) goto error_ret;

	lst_new(&aci->_ses_list);


error_ret:
	if(aci)
		mm_zfree(__the_acc_zone, aci);
	return 0;
}

long net_acceptor_destroy(struct acceptor* acc)
{

error_ret:
	return -1;
}

long net_acceptor_run(struct acceptor* acc)
{
error_ret:
	return -1;
}

long net_send(struct session* se)
{

error_ret:
	return -1;
}

