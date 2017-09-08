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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define NET_ACC_ZONE_NAME "net_acc_zone"
#define NET_SES_ZONE_NAME "net_ses_zone"
#define NET_SEND_DATA_ZONE_NAME "net_send_data_zone"

#define NET_BACKLOG_COUNT 1024
#define MSG_SIZE_LEN (2)

#define ACC_TYPE_INFO (0x11335577)
#define SES_TYPE_INFO (0x22446688)

enum _SESSION_STATE
{
	_SES_INVALID,
	_SES_NORMAL,
	_SES_CLOSING,
	_SES_CLOSED,
};

struct _inet_impl
{
	struct internet _the_net;
	struct epoll_event* _ep_ev;
	struct dlist _acc_list;
	struct dlist _ses_list;

	int _epoll_fd;
};

struct _acc_impl
{
	unsigned int _type_info;
	int _sock_fd;

	struct _inet_impl* _inet;
	struct acceptor _the_acc;
	struct dlnode _lst_node;
};

struct _ses_impl
{
	unsigned int _type_info;
	int _sock_fd;

	struct session _the_ses;
	struct dlnode _lst_node;
	struct dlist _send_list;
	struct session_ops _ops;

	char* _recv_buf;

	unsigned int _bytes_recv;
	unsigned int _recv_buf_len;

	struct _inet_impl* _inet;

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

static long _net_on_acc(struct _inet_impl* inet, struct _acc_impl* aci);
static long _net_close(struct _ses_impl* sei);
static void _net_on_recv(struct _inet_impl* inet, struct _ses_impl* sei);

static void _net_on_error(struct _inet_impl* inet, struct _ses_impl* sei);
static long _net_try_send_all(struct _ses_impl* sei);
static void _net_on_send(struct _inet_impl* inet, struct _ses_impl* sei);
static long _net_disconn(struct _inet_impl* inet, struct _ses_impl* sei);


static inline struct _inet_impl* _conv_inet_impl(struct internet* inet)
{
	return (struct _inet_impl*)((unsigned long)inet - (unsigned long)&((struct _inet_impl*)(0))->_the_net);
}

static inline struct _acc_impl* _conv_acc_impl(struct acceptor* acc)
{
	return (struct _acc_impl*)((unsigned long)acc - (unsigned long)&((struct _acc_impl*)(0))->_the_acc);
}

static inline struct _ses_impl* _conv_ses_impl(struct session* ses)
{
	return (struct _ses_impl*)((unsigned long)ses - (unsigned long)&((struct _ses_impl*)(0))->_the_ses);
}

static inline struct _acc_impl* _conv_acc_dln(struct dlnode* dln)
{
	return (struct _acc_impl*)((unsigned long)dln - (unsigned long)&((struct _acc_impl*)(0))->_lst_node);
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
			__the_acc_zone = mm_zcreate(NET_ACC_ZONE_NAME, sizeof(struct _acc_impl), 0, 0);
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
			__the_ses_zone = mm_zcreate(NET_SES_ZONE_NAME, sizeof(struct _ses_impl), 0, 0);
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
			__the_send_data_zone = mm_zcreate(NET_SEND_DATA_ZONE_NAME, sizeof(struct _send_data_node), 0, 0);
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

struct internet* internet_create(const struct net_config* cfg, const struct net_ops* ops)
{
	long rslt;
	struct _inet_impl* inet = 0;

	rslt = _net_try_restore_zones();
	if(rslt < 0) goto error_ret;

	inet = mm_area_alloc(sizeof(struct _inet_impl), MM_AREA_PERSIS);
	if(!inet) goto error_ret;

	inet->_ep_ev = mm_area_alloc(cfg->max_conn_count * sizeof(struct epoll_event), MM_AREA_PERSIS);
	if(!inet->_ep_ev) goto error_ret;

	inet->_epoll_fd = epoll_create(cfg->max_conn_count);
	if(inet->_epoll_fd < 0) goto error_ret;

	lst_new(&inet->_acc_list);
	lst_new(&inet->_ses_list);

	inet->_the_net.cfg.send_buff_len = cfg->send_buff_len;
	inet->_the_net.cfg.recv_buff_len = cfg->recv_buff_len;
	inet->_the_net.cfg.max_conn_count = cfg->max_conn_count;

	inet->_the_net.ops.func_acc = ops->func_acc;
	inet->_the_net.ops.func_conn = ops->func_conn;
	inet->_the_net.ops.func_recv = ops->func_recv;
	inet->_the_net.ops.func_disconn = ops->func_disconn;

	return &inet->_the_net;
error_ret:
	if(inet)
	{
		if(inet->_epoll_fd > 0)
			close(inet->_epoll_fd);
		if(inet->_ep_ev)
			mm_free(inet->_ep_ev);
		mm_free(inet);
	}
	return 0;
}

struct acceptor* internet_create_acceptor(struct internet* net, unsigned int ip, unsigned short port)
{
	long rslt;
	int sock_opt;
	struct _acc_impl* aci;
	struct _inet_impl* inet;
	struct sockaddr_in addr;
	struct epoll_event ev;

	if(!net) goto error_ret;

	inet = _conv_inet_impl(net);

	aci = mm_zalloc(__the_acc_zone);
	if(!aci) goto error_ret;

	aci->_sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if(aci->_sock_fd < 0) goto error_ret;

	sock_opt = 1;
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(int));
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_RCVBUF, &net->cfg.recv_buff_len, sizeof(int));
	rslt = setsockopt(aci->_sock_fd, SOL_SOCKET, SO_SNDBUF, &net->cfg.send_buff_len, sizeof(int));

	ip = htonl(ip);

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr = *(struct in_addr*)&ip;
	addr.sin_port = htons(port);

	rslt = bind(aci->_sock_fd, (struct sockaddr*)&addr, sizeof(addr));
	if(rslt < 0) goto error_ret;

	rslt = listen(aci->_sock_fd, NET_BACKLOG_COUNT);
	if(rslt < 0) goto error_ret;

	aci->_type_info = ACC_TYPE_INFO;

	ev.events = EPOLLIN;
	ev.data.ptr = aci;

	rslt = epoll_ctl(inet->_epoll_fd, EPOLL_CTL_ADD, aci->_sock_fd, &ev);
	if(rslt < 0) goto error_ret;

	aci->_inet = inet;

	lst_clr(&aci->_lst_node);
	lst_push_back(&inet->_acc_list, &aci->_lst_node);

	return &aci->_the_acc;
error_ret:
	internet_destroy_acceptor(&aci->_the_acc);
	return 0;
}

long internet_destroy(struct internet* net)
{
	struct dlnode *dln, *rmv_dln;
	struct _inet_impl* inet;
	if(!inet) goto error_ret;

	inet = _conv_inet_impl(net);

	close(inet->_epoll_fd);
	mm_free(inet->_ep_ev);

	dln = inet->_acc_list.head.next;

	while(dln != &inet->_acc_list.tail)
	{
		struct _acc_impl* aci = _conv_acc_dln(dln);
		rmv_dln = dln;
		dln = dln->next;

		internet_destroy_acceptor(&aci->_the_acc);
		lst_remove(&inet->_acc_list, rmv_dln);
	}

	dln = inet->_ses_list.head.next;

	while(dln != &inet->_ses_list.tail)
	{
		struct _ses_impl* sei = _conv_ses_dln(dln);
		rmv_dln = dln;
		dln = dln->next;

		_net_close(sei);

		lst_remove(&inet->_acc_list, rmv_dln);
	}

	mm_free(inet);

	return 0;
error_ret:
	return -1;
}

long internet_destroy_acceptor(struct acceptor* acc)
{
	struct _acc_impl* aci;
	struct _inet_impl* inet;

	if(!acc) goto error_ret;

	aci = _conv_acc_impl(acc);
	inet = aci->_inet;

	epoll_ctl(inet->_epoll_fd, EPOLL_CTL_DEL, aci->_sock_fd, 0);

	close(aci->_sock_fd);
	aci->_type_info = 0;

	return mm_zfree(__the_acc_zone, aci);
error_ret:
	return -1;
}

static inline struct _ses_impl* _net_create_session(struct _inet_impl* inet, int socket_fd)
{
	long rslt;
	int sock_opt;
	struct _ses_impl* sei = 0;
	struct epoll_event ev;

	sei = mm_zalloc(__the_ses_zone);
	if(!sei) goto error_ret;

	sei->_state = _SES_INVALID;
	sei->_recv_buf = 0;

	lst_clr(&sei->_lst_node);
	lst_new(&sei->_send_list);

	sei->_sock_fd = socket_fd;
	sei->_inet = inet;
	sei->_type_info = SES_TYPE_INFO;

	sock_opt = 1;
	rslt = setsockopt(sei->_sock_fd, SOL_SOCKET, SO_KEEPALIVE, &sock_opt, sizeof(int));
	rslt = setsockopt(sei->_sock_fd, SOL_SOCKET, SO_RCVBUF, &inet->_the_net.cfg.recv_buff_len, sizeof(int));
	rslt = setsockopt(sei->_sock_fd, SOL_SOCKET, SO_SNDBUF, &inet->_the_net.cfg.send_buff_len, sizeof(int));

	sei->_recv_buf_len = inet->_the_net.cfg.recv_buff_len;
	sei->_recv_buf = mm_alloc(sei->_recv_buf_len);
	if(!sei->_recv_buf) goto error_ret;

	ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;
	ev.data.ptr = sei;

	rslt = epoll_ctl(inet->_epoll_fd, EPOLL_CTL_ADD, sei->_sock_fd, &ev);
	if(rslt < 0) goto error_ret;

	sei->_state = _SES_NORMAL;
	lst_push_back(&inet->_ses_list, &sei->_lst_node);

	return sei;
error_ret:
	if(sei)
	{
		_net_close(sei);
	}
	return 0;
}

static long _net_on_acc(struct _inet_impl* inet, struct _acc_impl* aci)
{
	long rslt;
	int new_sock, sock_opt;
	socklen_t addr_len = 0;
	struct sockaddr_in remote_addr;
	struct _ses_impl* sei = 0;
	struct epoll_event ev;

	new_sock = accept4(aci->_sock_fd, (struct sockaddr*)&remote_addr, &addr_len, 0);
	if(new_sock < 0) goto error_ret;

	sei = _net_create_session(inet, new_sock);
	if(!sei) goto error_ret;

	sei->_the_ses.remote_ip = ntohl(*(unsigned int*)&remote_addr.sin_addr);
	sei->_the_ses.remote_port = ntohs(remote_addr.sin_port);

	if(inet->_the_net.ops.func_acc)
		(*inet->_the_net.ops.func_acc)(&aci->_the_acc, &sei->_the_ses);

	return 0;
error_ret:
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

static void _net_on_recv(struct _inet_impl* inet, struct _ses_impl* sei)
{
	long rslt;
	int recv_len = 0;

	char* p = sei->_recv_buf;
	sei->_bytes_recv = 0;
	on_recv_func rf;

	if(!sei->_recv_buf || sei->_recv_buf_len <= 0 || sei->_state != _SES_NORMAL)
		goto error_ret;

	rf = sei->_ops.func_recv ? sei->_ops.func_recv : inet->_the_net.ops.func_recv;

	do
	{
		recv_len = recv(sei->_sock_fd, p, sei->_recv_buf_len - sei->_bytes_recv, MSG_DONTWAIT);
		if(recv_len <= 0) break;

		p += recv_len;
		sei->_bytes_recv += recv_len;

		if(sei->_bytes_recv >= sei->_recv_buf_len)
		{
			if(rf)
				(*rf)(&sei->_the_ses, sei->_recv_buf, sei->_bytes_recv);

			p = sei->_recv_buf;
			sei->_bytes_recv = 0;
		}
	}
	while(recv_len > 0);

	if(recv_len < 0)
	{
		if(errno != EWOULDBLOCK && errno != EAGAIN)
			goto error_ret;
		else if(rf)
			(*rf)(&sei->_the_ses, sei->_recv_buf, sei->_bytes_recv);
	}
	else if(recv_len == 0)
		_net_disconn(inet, sei);

	return;
error_ret:
	_net_close(sei);
	return;
}

static inline void _net_on_error(struct _inet_impl* inet, struct _ses_impl* sei)
{
	_net_disconn(inet, sei);
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

static inline void _net_on_send(struct _inet_impl* inet, struct _ses_impl* sei)
{
	_net_try_send_all(sei);
}

long internet_send(struct session* ses, const char* data, int data_len)
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

static inline long _net_disconn(struct _inet_impl* inet, struct _ses_impl* sei)
{
	long rslt;
	on_disconn_func df;

	if(sei->_state != _SES_NORMAL)
		goto error_ret;

	df = sei->_ops.func_disconn ? sei->_ops.func_disconn : inet->_the_net.ops.func_disconn;

	if(df)
		(*df)(&sei->_the_ses);

	shutdown(sei->_sock_fd, SHUT_RD);

	rslt = epoll_ctl(inet->_epoll_fd, EPOLL_CTL_DEL, sei->_sock_fd, 0);
	if(rslt < 0) goto error_ret;

	sei->_state = _SES_CLOSING;
	_net_try_send_all(sei);

	return 0;
error_ret:
	return -1;
}

long internet_disconnect(struct session* ses)
{
	long rslt;
	struct _ses_impl* sei;

	if(!ses) goto error_ret;
	sei = _conv_ses_impl(ses);

	return _net_disconn(sei->_inet, sei);
error_ret:
	return -1;
}

long internet_run(struct internet* net)
{
	long rslt, cnt;
	struct _inet_impl* inet;
	struct _ses_impl* sei;

	if(!net) goto error_ret;
	inet = _conv_inet_impl(net);

	cnt = epoll_wait(inet->_epoll_fd, inet->_ep_ev, inet->_the_net.cfg.max_conn_count, -1);
	if(cnt < 0) goto error_ret;

	for(long i = 0; i < cnt; ++i)
	{
		unsigned int type_info = *(unsigned int*)(inet->_ep_ev[i].data.ptr);
		if(type_info == ACC_TYPE_INFO)
		{
			rslt = _net_on_acc(inet, (struct _acc_impl*)inet->_ep_ev[i].data.ptr);
			if(rslt < 0) goto error_ret;
		}
		else if(type_info == SES_TYPE_INFO)
		{
			sei = (struct _ses_impl*)inet->_ep_ev[i].data.ptr;

			if(inet->_ep_ev[i].events & EPOLLIN)
				_net_on_recv(inet, sei);
			if(inet->_ep_ev[i].events & EPOLLOUT)
				_net_on_send(inet, sei);
			if(inet->_ep_ev[i].events & EPOLLERR)
				_net_on_error(inet, sei);
		}
	}

	return 0;
error_ret:
	return -1;

}

struct session* internet_connect(struct internet* net, unsigned int ip, unsigned short port)
{
	long rslt;
	int new_sock;

	struct _inet_impl* inet;
	struct _ses_impl* sei;
	struct sockaddr_in addr;

	if(!net) goto error_ret;

	inet = _conv_inet_impl(net);

	new_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if(new_sock < 0) goto error_ret;

	sei = _net_create_session(inet, new_sock);
	if(!sei) goto error_ret;

	addr.sin_family = AF_INET;
	addr.sin_addr = *(struct in_addr*)&ip;
	addr.sin_port = htons(port);

	rslt = connect(sei->_sock_fd, (struct sockaddr*)&addr, sizeof(addr));
	if(rslt < 0 && errno != EINPROGRESS) goto error_ret;

	if(net->ops.func_conn)
		(*net->ops.func_conn)(&sei->_the_ses);

	return &sei->_the_ses;

error_ret:
	perror(strerror(errno));
	if(sei)
		_net_close(sei);
	return 0;
}

inline long internet_bind_session_ops(struct session* ses, const struct session_ops* ops)
{
	struct _ses_impl* sei;

	if(!ses) goto error_ret;
	sei = _conv_ses_impl(ses);

	if(ops)
	{
		sei->_ops.func_recv = ops->func_recv;
		sei->_ops.func_disconn = ops->func_disconn;
	}
	else
	{
		sei->_ops.func_recv = 0;
		sei->_ops.func_disconn = 0;
	}

	return 0;
error_ret:
	return -1;
}

