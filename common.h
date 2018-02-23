/*
 * common.h
 *
 *  Created on: Jul 29, 2017
 *      Author: wangyu
 */

#pragma once

//#ifndef COMMON_H_
//#define COMMON_H_
//#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<getopt.h>

#include<unistd.h>
#include<errno.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/socket.h>    //for socket ofcourse
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h> //for exit(0);
#include <errno.h> //For errno - the error number
#include <netinet/tcp.h>   //Provides declarations for tcp header
#include <netinet/udp.h>
#include <netinet/ip.h>    //Provides declarations for ip header
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <byteswap.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <sys/time.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <assert.h>
#include <linux/if_packet.h>
#include <linux/if_tun.h>

#include<unordered_map>
#include<unordered_set>
#include<map>
#include<list>
#include <memory>
#include <vector>
//#include <pair>
using  namespace std;


typedef unsigned long long u64_t;   //this works on most platform,avoid using the PRId64
typedef long long i64_t;

typedef unsigned int u32_t;
typedef int i32_t;

typedef unsigned short u16_t;
typedef short i16_t;

typedef u64_t my_time_t;

//const int max_data_len=2200;
//const int buf_len=max_data_len+200;

const int max_addr_len=100;

const int max_data_len_udp=65536;
const int max_data_len_tcp=4096*4;

const u32_t conn_timeout_udp=180000;
const u32_t conn_timeout_tcp=360000;
const int max_conn_num=20000;

const int conn_clear_ratio=30;
const int conn_clear_min=1;

const u32_t conn_clear_interval=1000;

const u32_t timer_interval=400;//this should be smaller than heartbeat_interval and retry interval;

extern int about_to_exit;

extern int socket_buf_size;

typedef u64_t fd64_t;

struct not_copy_able_t
{
	not_copy_able_t()
	{

	}
	not_copy_able_t(const not_copy_able_t &other)
	{
		assert(0==1);
	}
	not_copy_able_t & operator=(const not_copy_able_t &other)
	{
		assert(0==1);
	}
};

struct tcp_info_t:not_copy_able_t
{
	fd64_t fd64;
	epoll_event ev;
	char * data;
	//char data[max_data_len_tcp+200];//use a larger buffer than udp
	char * begin;
	int data_len;
	tcp_info_t()
	{
		data=(char*)malloc(max_data_len_tcp+200);

		begin=data;
		data_len=0;

	}
	~tcp_info_t()
	{
		if(data)
			delete data;
	}
	void free_memory()
	{
		delete data;
		data=0;
		begin=0;
	}
};

u32_t djb2(unsigned char *str,int len);
u32_t sdbm(unsigned char *str,int len);

struct address_t  //TODO scope id
{
	struct hash_function
	{
	    u32_t operator()(const address_t &key) const
		{
	    	return sdbm((unsigned char*)&key.inner,sizeof(key.inner));
		}
	};

	union storage_t //sockaddr_storage is too huge, we dont use it.
	{
		sockaddr_in ipv4;
		sockaddr_in6 ipv6;
	};
	storage_t inner;

	address_t()
	{
		clear();
	}
	void clear()
	{
		memset(&inner,0,sizeof(inner));
	}
	int from_str(char * str);

	int from_sockaddr(sockaddr *,socklen_t);

	char* get_str();
	void to_str(char *);

	inline u32_t get_type()
	{
		return ((sockaddr*)&inner)->sa_family;
	}

	inline u32_t get_len()
	{
		u32_t type=get_type();
		switch(type)
		{
			case AF_INET:
				return sizeof(sockaddr_in);
			case AF_INET6:
				return sizeof(sockaddr_in6);
			default:
				assert(0==1);
		}
		return -1;
	}

    bool operator == (const address_t &b) const
    {
    	//return this->data==b.data;
        return memcmp(&this->inner,&b.inner,sizeof(this->inner))==0;
    }

    int new_connected_udp_fd();
};


struct udp_pair_t:not_copy_able_t
{
	address_t adress;
	fd64_t fd64;
	u64_t last_active_time;
	char addr_s[max_addr_len];
	list<udp_pair_t>::iterator it;
	udp_pair_t()
	{
		addr_s[0]=0;
	}
	//int not_used=0;
};

struct tcp_pair_t:not_copy_able_t
{
	tcp_info_t local;
	tcp_info_t remote;
	u64_t last_active_time;
	list<tcp_pair_t>::iterator it;
	char addr_s[max_addr_len];
	int not_used=0;
	tcp_pair_t()
	{
		addr_s[0]=0;
	}
};

struct fd_info_t:not_copy_able_t
{
	int is_tcp=0;
	tcp_pair_t *tcp_pair_p=0;
	udp_pair_t *udp_pair_p=0;
};


u64_t get_current_time();
u64_t get_current_time_us();
u64_t pack_u64(u32_t a,u32_t b);
u32_t get_u64_h(u64_t a);
u32_t get_u64_l(u64_t a);

void write_u16(char *,u16_t a);
u16_t read_u16(char *);

void write_u32(char *,u32_t a);
u32_t read_u32(char *);

void write_u64(char *,u64_t a);
u64_t read_uu64(char *);

char * my_ntoa(u32_t ip);

void myexit(int a);
void init_random_number_fd();
u64_t get_true_random_number_64();
u32_t get_true_random_number();
u32_t get_true_random_number_nz();
u64_t ntoh64(u64_t a);
u64_t hton64(u64_t a);
bool larger_than_u16(uint16_t a,uint16_t b);
bool larger_than_u32(u32_t a,u32_t b);
void setnonblocking(int sock);

int set_buf_size(int fd,int socket_buf_size,int force_socket_buf=0);

void  signal_handler(int sig);

void get_true_random_chars(char * s,int len);
int random_between(u32_t a,u32_t b);


int round_up_div(int a,int b);

int set_timer(int epollfd,int &timer_fd);

//#endif /* COMMON_H_ */
