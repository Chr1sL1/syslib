#include "net.h"
#include "mmspace.h"
#include "misc.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define USR_SESSION_ZONE_NAME "test_usr_session_zone"
#define USR_TYPE_INFO (0x123123)
#define MAX_LIVE_INTERVAL (10000)

#define TEST_CONN_COUNT (1024)

static struct mmzone* __usr_session_zone = 0;
static long __running = 1;
static unsigned long __time_val = 0;

static unsigned long __recv_bytes = 0;
static unsigned long __send_bytes = 0;

enum USR_SESSION_STATE
{
	USS_DISCONNECTED = 0,
	USS_ESTABLISHING,
	USS_RUNNING,

	USS_COUNT,
};

static struct net_config __cfg =
{
	.max_fd_count = 2048,
	.recv_buff_len = 1024,
	.send_buff_len = 1024,
};

struct usr_session
{
	struct session* s;
	unsigned int idx;
	unsigned int state;
	unsigned long type_info;
	unsigned long conn_tick;
	unsigned long disconn_tick;
	unsigned long live_interval;
	unsigned long sleep_interval;
};

static struct usr_session __conn_session[TEST_CONN_COUNT] =
{
	[0 ... TEST_CONN_COUNT - 1] = {
		.s = 0,
		.idx = 0,
		.state = USS_DISCONNECTED,
		.type_info = USR_TYPE_INFO,
		.conn_tick = 0,
		.disconn_tick = 0,
		.live_interval = 0,
		.sleep_interval = 0,
	}
};

static void _usr_session_ctor(void* ptr)
{
	struct usr_session* us = (struct usr_session*)ptr;
	us->type_info = USR_TYPE_INFO;
	us->conn_tick = 0;
}

static void _usr_session_dtor(void* ptr)
{
	struct usr_session* us = (struct usr_session*)ptr;
	err_exit(us->type_info != USR_TYPE_INFO, "fatal error: usr_session_dtor.");

error_ret:
	return;
}

static void _signal_stop(int sig, siginfo_t* t, void* usr_data)
{
	__running = 0;
}

static long _restore_zone(void)
{
	if(!__usr_session_zone)
		__usr_session_zone = mm_search_zone(USR_SESSION_ZONE_NAME);

	if(__usr_session_zone && __usr_session_zone->obj_size != sizeof(struct usr_session))
		goto error_ret;

	__usr_session_zone = mm_zcreate(USR_SESSION_ZONE_NAME, sizeof(struct usr_session), _usr_session_ctor, _usr_session_dtor);

	if(!__usr_session_zone)
		goto error_ret;

	return 0;
error_ret:
	return -1;
}

static long on_acc(struct acceptor* acc, struct session* se)
{
	long rslt;
	struct usr_session* us;

	us = mm_zalloc(__usr_session_zone);
	err_exit(!us, "on_acc alloc session failed.");

	us->conn_tick = __time_val;

	se->usr_ptr = us;

	return 0;
error_ret:
	return -1;
}

static long on_server_disconn(struct session* se)
{
	long rslt;
	struct usr_session* us = (struct usr_session*)se->usr_ptr;

	rslt = mm_zfree(__usr_session_zone, us);
	err_exit(rslt < 0, "on_disconn free session failed.");

	us->disconn_tick = __time_val;

	return 0;
error_ret:
	return -1;
}

static long on_server_recv(struct session* se, const void* buf, long len)
{
	__recv_bytes += len;

	return 0;
error_ret:
	return -1;
}


static long on_client_recv(struct session* se, const void* buf, long len)
{

	return 0;
error_ret:
	return -1;
}

static long on_client_conn(struct session* se)
{
	struct usr_session* us = (struct usr_session*)se->usr_ptr;
	err_exit(!us, "strange error in on_client_conn");

	us->state = USS_RUNNING;
	us->conn_tick = __time_val;
	us->live_interval = random() % MAX_LIVE_INTERVAL;
	us->sleep_interval = random() % MAX_LIVE_INTERVAL / 2;

//	printf("session connected [%d]\n", us->idx);

	return 0;
error_ret:
	return -1;
}

static long on_client_disconn(struct session* se)
{
	struct usr_session* us = (struct usr_session*)se->usr_ptr;
	err_exit(!us, "strange error in on_client_disconn");

	us->state = USS_DISCONNECTED;

//	printf("client session disconnected [%d]\n", us->idx);

	return 0;
error_ret:
	return -1;
}

static void fill_send_data(char* buf, int size)
{
	for(int i = 0; i < size; i++)
	{
		buf[i] = random() % 255;
	}
}

static long run_connector(struct net_struct* net)
{
	struct session_ops ops = 
	{
		.func_conn = on_client_conn,
		.func_recv = on_client_recv,
		.func_disconn = on_client_disconn,
	};

	char send_buf[net->cfg.send_buff_len];

	for(int i = 0; i < TEST_CONN_COUNT; ++i)
	{
		if(__conn_session[i].state == USS_DISCONNECTED)
		{
			if(__time_val - __conn_session[i].conn_tick < __conn_session[i].sleep_interval)
				continue;

			__conn_session[i].idx = i;

			struct session* s = net_connect(net, inet_addr("192.168.1.3"), 7070);
			err_exit(!s, "connect failed [%d]", i);

			net_bind_session_ops(s, &ops);

			__conn_session[i].state = USS_ESTABLISHING;
			__conn_session[i].s = s;

			s->usr_ptr = &__conn_session[i];
		}
		else if(__conn_session[i].state == USS_RUNNING)
		{
			if(__time_val - __conn_session[i].conn_tick > __conn_session[i].live_interval)
			{
				net_disconnect(__conn_session[i].s);
				continue;
			}

			fill_send_data(send_buf, net->cfg.send_buff_len);

			net_send(__conn_session[i].s, send_buf, net->cfg.send_buff_len - 1);
			__send_bytes += (net->cfg.send_buff_len - 1);
		}
	}

	return 0;
error_ret:
	return -1;
}

long net_test_server(void)
{
	long rslt;

	unsigned long count_tick = 0;

	struct net_ops ops;
	struct net_struct* net;
	struct acceptor* acc;

	struct sigaction sa;
	sa.sa_sigaction = _signal_stop;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGINT, &sa, 0);

	rslt = _restore_zone();
	err_exit(rslt < 0, "restore zone failed.");

	ops.func_recv = on_server_recv;
	ops.func_disconn = on_server_disconn;
	ops.func_acc = on_acc;
	ops.func_conn = 0;

	net = net_create(&__cfg, &ops, NT_INTERNET);
	err_exit(!net, "create net failed.");

	acc = net_create_acceptor(net, 0, 7070);
	err_exit(!acc, "create acceptor failed.");

	while(__running)
	{
		struct timeval tv;

		gettimeofday(&tv, 0);

		__time_val = tv.tv_sec * 1000 + tv.tv_usec / 1000;

		rslt = run_connector(net);
		err_exit(rslt < 0, "run_connector failed.");

		rslt = net_run(net, 1000);
		err_exit(rslt < 0, "net_run failed.");

		if(__time_val > count_tick + 500)
		{
			count_tick = __time_val;
			printf(">>>>>>>>>>>>>>>>>>>>>> session count: %lu, recvd bytes: %lu.\n", net_session_count(net), __recv_bytes);
		}
	}

	net_destroy_acceptor(acc);
	net_destroy(net);

	return 0;
error_ret:
	return -1;
}
