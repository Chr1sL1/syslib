#include "net.h"
#include "mmspace.h"
#include "ringbuf.h"
#include "dlist.h"

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define NET_ACC_ZONE_NAME "net_acc_zone"
#define NET_SES_ZONE_NAME "net_ses_zone"

#define NET_BACKLOG_COUNT 1024
#define MSG_SIZE_LEN (2)

struct _acc_impl
{
	struct acceptor _the_acc;
	struct net_server_cfg _cfg;
	struct dlist _ses_list;
	struct epoll_event* _ep_ev;

	int _sock_fd;
	int _epoll_fd;
};

struct _ses_impl
{
	struct session _the_ses;
	struct dlnode _lst_node;
	struct _acc_impl* _aci;

	char* _recv_buf;
	char* _send_buf;

	int _bytes_recv;
	int _recv_buf_len;

	int _bytes_sent;
	int _send_buf_len;

	int _sock_fd;
};

static struct mmzone* __the_acc_zone = 0;
static struct mmzone* __the_ses_zone = 0;

static inline struct _acc_impl* _conv_acc_impl(struct acceptor* acc)
{
	return (struct _acc_impl*)((unsigned long)acc - (unsigned long)&((struct _acc_impl*)(0))->_the_acc);
}

static inline struct _ses_impl* _conv_ses_impl(struct session* ses)
{
	return (struct _ses_impl*)((unsigned long)ses - (unsigned long)&((struct _ses_impl*)(0))->_the_ses);
}

static inline struct _ses_impl* _conv_ses_dln(struct dlnode* dln)
{
	return (struct _ses_impl*)((unsigned long)dln - (unsigned long)&((struct _ses_impl*)(0))->_lst_node);
}

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

struct acceptor* net_create(unsigned int ip, unsigned short port, const struct net_server_cfg* cfg)
{
	long rslt;
	struct _acc_impl* aci;
	struct sockaddr_in addr;
	struct epoll_event ev;

	rslt = _net_try_restore_zones();
	if(rslt < 0) goto error_ret;

	aci = mm_zalloc(__the_acc_zone);
	if(!aci) goto error_ret;

	aci->_ep_ev = mm_area_alloc(cfg->max_conn_count * sizeof(struct epoll_event), MM_AREA_PERSIS);
	if(!aci->_ep_ev) goto error_ret;

	lst_new(&aci->_ses_list);

	aci->_sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if(aci->_sock_fd < 0) goto error_ret;

	ip = htonl(ip);

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr = *(struct in_addr*)&ip;
	addr.sin_port = htons(port);

	rslt = bind(aci->_sock_fd, (struct sockaddr*)&addr, sizeof(addr));
	if(rslt < 0) goto error_ret;

	rslt = listen(aci->_sock_fd, NET_BACKLOG_COUNT);
	if(rslt < 0) goto error_ret;

	aci->_epoll_fd = epoll_create(aci->_cfg.max_conn_count);
	if(aci->_epoll_fd < 0) goto error_ret;

	ev.events = EPOLLIN;
	ev.data.fd = aci->_sock_fd;

	rslt = epoll_ctl(aci->_epoll_fd, EPOLL_CTL_ADD, aci->_sock_fd, &ev);
	if(rslt < 0) goto error_ret;

	memcpy(&aci->_cfg, cfg, sizeof(struct net_server_cfg));

	return &aci->_the_acc;
error_ret:
	if(aci->_epoll_fd > 0)
		close(aci->_epoll_fd);
	if(aci->_sock_fd > 0)
		close(aci->_sock_fd);
	if(aci->_ep_ev)
		mm_free(aci->_ep_ev);
	if(aci)
		mm_zfree(__the_acc_zone, aci);
	return 0;
}

long net_destroy(struct acceptor* acc)
{
	struct _acc_impl* aci;
	if(!acc) goto error_ret;

	aci = _conv_acc_impl(acc);

	// todo: close all sessions and clear session list.

	
	if(aci->_epoll_fd > 0)
		close(aci->_epoll_fd);

	if(aci->_sock_fd > 0)
		close(aci->_sock_fd);

	return 0;
error_ret:
	return -1;
}

inline long net_set_recv_buf(struct session* se, char* buf, int size)
{
	if(!se) goto error_ret;

	struct _ses_impl* sei = _conv_ses_impl(se);

	sei->_recv_buf = buf;
	sei->_recv_buf_len = size;
	sei->_bytes_recv = 0;

	return 0;
error_ret:
	return -1;
}

inline long net_set_send_buf(struct session* se, char* buf, int size)
{
	if(!se) goto error_ret;

	struct _ses_impl* sei = _conv_ses_impl(se);

	sei->_send_buf = buf;
	sei->_send_buf_len = size;
	sei->_bytes_sent = 0;

	return 0;
error_ret:
	return -1;

}

static long _net_on_acc(struct _acc_impl* aci)
{
	long rslt;
	int new_sock;
	socklen_t addr_len;
	struct sockaddr_in remote_addr;
	struct _ses_impl* sei;
	struct epoll_event ev;

	new_sock = accept4(aci->_sock_fd, (struct sockaddr*)&remote_addr, &addr_len, SOCK_NONBLOCK);
	if(new_sock < 0) goto error_ret;

	sei = mm_zalloc(__the_ses_zone);
	if(!sei) goto error_ret;

	lst_clr(&sei->_lst_node);

	sei->_sock_fd = new_sock;
	sei->_aci = aci;

	sei->_recv_buf = 0;
	sei->_send_buf = 0;

	sei->_bytes_recv = 0;
	sei->_recv_buf_len = 0 ;

	sei->_bytes_sent = 0;
	sei->_send_buf_len = 0;

	ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
	ev.data.ptr = sei;

	rslt = epoll_ctl(aci->_epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);
	if(rslt < 0) goto error_ret;

	if(aci->_cfg.func_acc)
		(*aci->_cfg.func_acc)(&aci->_the_acc, &sei->_the_ses);

	return 0;
error_ret:
	if(sei->_sock_fd)
		close(sei->_sock_fd);
	if(sei)
		mm_zfree(__the_ses_zone, sei);
	return -1;
}

static long _net_on_recv(struct _acc_impl* aci, struct _ses_impl* sei)
{
	long rslt;
	int recv_len = 0;
	char* p = sei->_recv_buf;
	int remain_size = sei->_recv_buf_len;

	if(!sei->_recv_buf || sei->_recv_buf_len <= 0) goto error_ret;

	while(recv_len > 0)
	{
		recv_len = recv(sei->_sock_fd, p, remain_size, MSG_DONTWAIT);
		if(recv_len <= 0) break;

		p += recv_len;
		remain_size -= recv_len;

		if(remain_size <= 0)
		{
			if(aci->_cfg.func_recv)
				(*aci->_cfg.func_recv)(&sei->_the_ses, sei->_recv_buf, sei->_recv_buf_len);

			p = sei->_recv_buf;
			remain_size = sei->_recv_buf_len;
		}
	}

	if(errno != EWOULDBLOCK && errno != EAGAIN)
		goto error_ret;
	else if(aci->_cfg.func_recv)
		(*aci->_cfg.func_recv)(&sei->_the_ses, sei->_recv_buf, sei->_recv_buf_len - remain_size);

	return 0;
error_ret:
	net_disconnect(&sei->_the_ses);
	return -1;
}

static long _net_on_send(struct _acc_impl* aci, struct _ses_impl* sei)
{

	return 0;
error_ret:
	return -1;
}

static long _net_on_error(struct _acc_impl* aci, struct _ses_impl* sei)
{

	return 0;
error_ret:
	return -1;
}

long net_run(struct acceptor* acc)
{
	long rslt, cnt;
	struct _acc_impl* aci;
	struct _ses_impl* sei;
	if(!acc) goto error_ret;

	aci = _conv_acc_impl(acc);

	cnt = epoll_wait(aci->_epoll_fd, aci->_ep_ev, aci->_cfg.max_conn_count, -1);
	if(cnt < 0) goto error_ret;

	for(long i = 0; i < cnt; ++i)
	{
		if(aci->_sock_fd == aci->_ep_ev[i].data.fd)
		{
			rslt = _net_on_acc(aci);
			continue;
		}

		sei = (struct _ses_impl*)aci->_ep_ev[i].data.ptr;

		if(aci->_ep_ev[i].events & EPOLLIN)
			_net_on_recv(aci, sei);
		if(aci->_ep_ev[i].events & EPOLLOUT)
			_net_on_send(aci, sei);
		if(aci->_ep_ev[i].events & EPOLLERR)
			_net_on_error(aci, sei);
	}


	return 0;
error_ret:
	return -1;
}

struct session* net_connect(unsigned int ip, unsigned short port, const struct net_client_cfg* cfg)
{
	struct _ses_impl* ses;

	return &ses->_the_ses;
error_ret:
	return 0;
}


long net_send(struct session* ses)
{

error_ret:
	return -1;
}

long net_disconnect(struct session* se)
{

error_ret:
	return -1;
}

