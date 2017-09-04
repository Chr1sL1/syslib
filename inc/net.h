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

struct net_config
{
	unsigned int send_buff_len;
	unsigned int recv_buff_len;
	unsigned int max_conn_count;
};

struct net_ops
{
	on_acc_func func_acc;
	on_conn_func func_conn;
	on_recv_func func_recv;
	on_disconn_func func_disconn;
};

struct session_ops
{
	on_recv_func func_recv;
	on_disconn_func func_disconn;
};

struct internet
{
	struct net_config cfg;
	struct net_ops ops;
};

struct internet* internet_create(const struct net_config* cfg, const struct net_ops* ops);
long internet_destroy(struct internet* net);

struct acceptor* internet_create_acceptor(struct internet* net, unsigned int ip, unsigned short port);
long internet_destroy_acceptor(struct acceptor* acc);

struct session* internet_connect(struct internet* net, unsigned int ip, unsigned short port);
long internet_disconnect(struct session* ses);

long internet_send(struct session* ses, const char* data, int data_len);
long internet_run(struct internet* net);

long internet_bind_session_ops(struct session* ses, const struct session_ops* ops);


#endif

