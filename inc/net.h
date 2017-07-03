#ifndef __net_h__
#define __net_h__

struct listener
{
	unsigned int l_ip;
	unsigned int l_port;
};

struct session
{
	unsigned int r_ip;
	unsigned int r_port;
	unsigned int l_ip;
	unsigned int l_port;
};

// return 0 if succeed, return -1 if any error occurs.
typedef long (*on_acc_func)(struct listener* lt, struct session* se);
typedef long (*on_conn_func)(struct session* se);
typedef long (*on_disconn_func)(struct session* se);
typedef long (*on_recv_func)(struct session* se, const void* buf, long len);

struct net_io_cfg
{
	long send_buff_len;
	long recv_buff_len;
};

struct net_server_cfg
{
	struct net_io_cfg io_cfg;
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

struct listener* listener_create(unsigned int ip, unsigned int port, const net_server_cfg* cfg);
long listener_destroy(struct listener* ler);



#endif

