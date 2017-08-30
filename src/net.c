#include "net.h"
#include "mmspace.h"
#include "ring_buf.h"
#include "dlist.h"

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


struct acceptor* net_acceptor_create(unsigned int ip, unsigned int port, const struct net_server_cfg* cfg)
{

error_ret:
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

