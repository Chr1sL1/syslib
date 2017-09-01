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
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define NET_ACC_ZONE_NAME "net_acc_zone"
#define NET_SES_ZONE_NAME "net_ses_zone"
#define NET_SEND_DATA_ZONE_NAME "net_send_data_zone"

#define NET_BACKLOG_COUNT 1024
#define MSG_SIZE_LEN (2)

enum _SESSION_STATE
{
	_SES_INVALID,
	_SES_NORMAL,
	_SES_CLOSING,
	_SES_CLOSED,
};

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
	struct dlist _send_list;

	char* _recv_buf;
	char* _send_buf;

	unsigned int _bytes_recv;
	unsigned int _recv_buf_len;

	unsigned int _sent_remain;
	unsigned int _send_buf_len;

	int _sock_fd;
	int _state;
};

struct _send_data_node
{
	struct dlnode _lst_node;
	void* _data;
	int _data_size;
	int _data_sent;
};

static struct mmzone* __the_acc_zone = 0;
static struct mmzone* __the_ses_zone = 0;
static struct mmzone* __the_send_data_zone = 0;

static long _net_on_acc(struct _acc_impl* aci);
static long _net_close(struct _ses_impl* sei);
static void _net_on_recv(struct _acc_impl* aci, struct _ses_impl* sei);

static void _net_on_error(struct _acc_impl* aci, struct _ses_impl* sei);
static long _net_try_send_all(struct _ses_impl* sei);
static void _net_on_send(struct _acc_impl* aci, struct _ses_impl* sei);
static long _net_disconn(struct _ses_impl* sei);

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

static inline struct _send_data_node* _conv_send_data_dln(struct dlnode* dln)
{
	return (struct _send_data_node*)((unsigned long)dln - (unsigned long)&((struct _send_data_node*)(0))->_lst_node);
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
			if(__the_acc_zone->obj_size != sizeof(struct _acc_impl))
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
			if(__the_ses_zone->obj_size != sizeof(struct _ses_impl))
				goto error_ret;
		}
	}

	if(!__the_send_data_zone)
	{
		__the_send_data_zone = mm_search_zone(NET_SEND_DATA_ZONE_NAME);
		if(!__the_send_data_zone)
		{
			__the_send_data_zone = mm_zcreate(NET_SEND_DATA_ZONE_NAME, sizeof(struct _send_data_node));
			if(!__the_send_data_zone) goto error_ret;
		}
		else
		{
			if(__the_send_data_zone->obj_size != sizeof(struct _send_data_node))
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
	int sock_opt;
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

	sock_opt = 1;
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(int));
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_RCVBUF, &cfg->io_cfg.recv_buff_len, sizeof(int));
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_SNDBUF, &cfg->io_cfg.send_buff_len, sizeof(int));

	ip = htonl(ip);

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr = *(struct in_addr*)&ip;
	addr.sin_port = htons(port);

	rslt = bind(aci->_sock_fd, (struct sockaddr*)&addr, sizeof(addr));
	if(rslt < 0) goto error_ret;

	rslt = listen(aci->_sock_fd, NET_BACKLOG_COUNT);
	if(rslt < 0) goto error_ret;

	aci->_epoll_fd = epoll_create(cfg->max_conn_count);
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

static long _net_on_acc(struct _acc_impl* aci)
{
	long rslt;
	int new_sock, sock_opt;
	socklen_t addr_len = 0;
	struct sockaddr_in remote_addr;
	struct _ses_impl* sei = 0;
	struct epoll_event ev;

	new_sock = accept4(aci->_sock_fd, (struct sockaddr*)&remote_addr, &addr_len, 0);
	if(new_sock < 0) goto error_ret;

	sock_opt = 1;
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(int));
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_RCVBUF, &aci->_cfg.io_cfg.recv_buff_len, sizeof(int));
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_SNDBUF, &aci->_cfg.io_cfg.send_buff_len, sizeof(int));

	sei = mm_zalloc(__the_ses_zone);
	if(!sei) goto error_ret;

	sei->_state = _SES_INVALID;

	lst_clr(&sei->_lst_node);
	lst_new(&sei->_send_list);

	sei->_sock_fd = new_sock;
	sei->_aci = aci;

	sei->_the_ses.remote_ip = ntohl(*(unsigned int*)&remote_addr.sin_addr);
	sei->_the_ses.remote_port = ntohs(remote_addr.sin_port);

	sei->_recv_buf = 0;
	sei->_send_buf = 0;

	sei->_recv_buf_len = aci->_cfg.io_cfg.recv_buff_len;
	sei->_send_buf_len = aci->_cfg.io_cfg.send_buff_len;

	sei->_recv_buf = mm_alloc(sei->_recv_buf_len);
	if(!sei->_recv_buf) goto error_ret;

	sei->_send_buf = mm_alloc(sei->_send_buf_len);
	if(!sei->_send_buf) goto error_ret;

	ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
	ev.data.ptr = sei;

	rslt = epoll_ctl(aci->_epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);
	if(rslt < 0) goto error_ret;

	sei->_state = _SES_NORMAL;

	if(aci->_cfg.func_acc)
		(*aci->_cfg.func_acc)(&aci->_the_acc, &sei->_the_ses);

	return 0;
error_ret:
	perror(strerror(errno));
	if(sei)
	{
		if(sei->_recv_buf)
			mm_free(sei->_recv_buf);
		if(sei->_send_buf)
			mm_free(sei->_send_buf);
		if(sei->_sock_fd)
			close(sei->_sock_fd);
			mm_zfree(__the_ses_zone, sei);
	}
	return -1;
}

static long _net_close(struct _ses_impl* sei)
{
	struct dlnode* dln;

	close(sei->_sock_fd);

	if(sei->_recv_buf)
		mm_free(sei->_recv_buf);

	if(!lst_empty(&sei->_send_list))
	{
		dln = sei->_send_list.head.next;

		while(dln != &sei->_send_list.tail)
		{
			struct _send_data_node* sdn = _conv_send_data_dln(dln);
			dln = dln->next;

			mm_free(sdn->_data);
			mm_zfree(__the_send_data_zone, sdn);
		}
	}

	sei->_state = _SES_CLOSED;

	return mm_zfree(__the_ses_zone, sei);
}

static void _net_on_recv(struct _acc_impl* aci, struct _ses_impl* sei)
{
	long rslt;
	int recv_len = 0;

	char* p = sei->_recv_buf;
	sei->_bytes_recv = 0;

	if(!sei->_recv_buf || sei->_recv_buf_len <= 0 || sei->_state != _SES_NORMAL)
		goto error_ret;

	do
	{
		recv_len = recv(sei->_sock_fd, p, sei->_recv_buf_len - sei->_bytes_recv, MSG_DONTWAIT);
		if(recv_len <= 0) break;

		p += recv_len;
		sei->_bytes_recv += recv_len;

		if(sei->_bytes_recv >= sei->_recv_buf_len)
		{
			if(aci->_cfg.func_recv)
				(*aci->_cfg.func_recv)(&sei->_the_ses, sei->_recv_buf, sei->_bytes_recv);

			p = sei->_recv_buf;
			sei->_bytes_recv = 0;
		}
	}
	while(recv_len > 0);

	if(recv_len < 0)
	{
		if(errno != EWOULDBLOCK && errno != EAGAIN)
			goto error_ret;
		else if(aci->_cfg.func_recv)
			(*aci->_cfg.func_recv)(&sei->_the_ses, sei->_recv_buf, sei->_bytes_recv);
	}
	else if(recv_len == 0)
		_net_disconn(sei);

	return;
error_ret:
	_net_close(sei);
	return;
}

static inline void _net_on_error(struct _acc_impl* aci, struct _ses_impl* sei)
{
	_net_close(sei);
}


static long _net_try_send_all(struct _ses_impl* sei)
{
	int cnt = 0;
	struct dlnode* dln;
	struct dlnode* rmv_dln;

	dln = sei->_send_list.head.next;

	while(dln != &sei->_send_list.tail)
	{
		struct _send_data_node* sdn = _conv_send_data_dln(dln);

		while(sdn->_data_size > sdn->_data_sent)
		{
			cnt = send(sei->_sock_fd, sdn->_data + sdn->_data_sent, sdn->_data_size - sdn->_data_sent, MSG_DONTWAIT);
			if(cnt <= 0) goto send_finish;

			sdn->_data_sent += cnt;
		}

		rmv_dln = dln;
		dln = dln->next;

		lst_remove(&sei->_send_list, rmv_dln);

		mm_free(sdn->_data);
		mm_zfree(__the_send_data_zone, sdn);

	}

	if(sei->_state == _SES_CLOSING)
	{
		if(lst_empty(&sei->_send_list))
			_net_close(sei);
	}
	else
	{
send_finish:
		if(cnt < 0)
		{
			if(errno != EWOULDBLOCK && errno != EAGAIN)
				goto error_ret;
		}
	}

send_succ:
	return 0;
error_ret:
	_net_close(sei);
	return -1;
}

static inline void _net_on_send(struct _acc_impl* aci, struct _ses_impl* sei)
{
	_net_try_send_all(sei);
}

long net_send(struct session* ses, const char* data, int data_len)
{
	long rslt;
	struct _ses_impl* sei;
	struct _send_data_node* sdn = 0;

	if(!ses) goto error_ret;

	sei = _conv_ses_impl(ses);

	if(sei->_state != _SES_NORMAL)
		goto error_ret;

	sdn = mm_zalloc(__the_send_data_zone);
	if(!sdn) goto error_ret;

	lst_clr(&sdn->_lst_node);

	sdn->_data_size = data_len;

	sdn->_data = mm_alloc(data_len);
	if(!sdn->_data) goto error_ret;

	sdn->_data_sent = 0;

	memcpy(sdn->_data, data, data_len);

	rslt = lst_push_back(&sei->_send_list, &sdn->_lst_node);
	if(rslt < 0) goto error_ret;

	_net_try_send_all(sei);

	return 0;
error_ret:
	if(sdn)
	{
		if(sdn->_data)
			mm_free(sdn->_data);
		mm_zfree(__the_send_data_zone, sdn);
	}
	return -1;
}

static inline long _net_disconn(struct _ses_impl* sei)
{
	struct _acc_impl* aci = sei->_aci;

	if(sei->_state != _SES_NORMAL)
		goto error_ret;

	if(aci->_cfg.func_disconn)
		(*aci->_cfg.func_disconn)(&sei->_the_ses);

	shutdown(sei->_sock_fd, SHUT_RD);

	sei->_state = _SES_CLOSING;
	_net_try_send_all(sei);

	return 0;
error_ret:
	return -1;
}

long net_disconnect(struct session* ses)
{
	long rslt;
	struct _ses_impl* sei;

	if(!ses) goto error_ret;
	sei = _conv_ses_impl(ses);

	return _net_disconn(sei);
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

