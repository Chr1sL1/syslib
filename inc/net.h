#ifndef __net_h__
#define __net_h__

struct listener
{
	long idx;
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



#endif

