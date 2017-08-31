#ifndef __net_h__
#define __net_h__

struct acceptor 
{
	unsigned int local_ip;
	unsigned int local_port;
};

struct session
{
	unsigned int remote_ip;
	unsigned int remote_port;
	void* usr_ptr;
};

// return 0 if succeed, return -1 if any error occurs.
typedef long (*on_acc_func)(struct acceptor* acc, struct session* se);
typedef long (*on_conn_func)(struct session* se);
typedef long (*on_disconn_func)(struct session* se);
typedef long (*on_recv_func)(struct session* se, const void* buf, long len);

struct net_io_cfg
{
	unsigned long send_buff_len;
	unsigned long recv_buff_len;
};

struct net_server_cfg
{
	struct net_io_cfg io_cfg;
	unsigned long max_conn_count;

	on_acc_func func_acc;
	on_recv_func func_recv;
	on_disconn_func func_disconn;
};

struct net_client_cfg
{
	struct net_io_cfg io_cfg;
	on_conn_func func_conn;
	on_disconn_func func_disconn;
	on_recv_func func_recv;
};

struct acceptor* net_create(unsigned int ip, unsigned short port, const struct net_server_cfg* cfg);
long net_destroy(struct acceptor* acc);
long net_run(struct acceptor* acc);

struct session* net_connect(unsigned int ip, unsigned short port, const struct net_client_cfg* cfg);
long net_send(struct session* se);
long net_disconnect(struct session* se);

long net_set_recv_buf(struct session* se, char* buf, int size);
long net_set_send_buf(struct session* se, char* buf, int size);

#endif

