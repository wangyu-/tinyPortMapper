#include "common.h"
#include "log.h"
#include "git_version.h"
#include "fd_manager.h"

using  namespace std;

typedef unsigned long long u64_t;   //this works on most platform,avoid using the PRId64
typedef long long i64_t;

typedef unsigned int u32_t;
typedef int i32_t;

int local_listen_fd_tcp=-1;
int local_listen_fd_udp=-1;

int disable_conn_clear=0;

char local_address[100], remote_address[100];
u32_t remote_address_u32=0,local_address_u32=0;
int local_port = -1, remote_port = -1;

int max_pending_packet=0;
int enable_udp=0,enable_tcp=0;

int VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV;


struct conn_manager_t  //TODO change map to unordered map
{
	//typedef hash_map map;
	unordered_map<u64_t,u32_t> u64_to_fd;  //conv and u64 are both supposed to be uniq
	unordered_map<u32_t,u64_t> fd_to_u64;

	unordered_map<u32_t,u64_t> fd_last_active_time;

	unordered_map<u32_t,u64_t>::iterator clear_it;

	//void (*clear_function)(uint64_t u64) ;

	long long last_clear_time;
	conn_manager_t()
	{
		clear_it=fd_last_active_time.begin();
		long long last_clear_time=0;
		rehash();
		//clear_function=0;
	}
	~conn_manager_t()
	{
		clear();
	}
	int get_size()
	{
		return fd_to_u64.size();
	}
	void rehash()
	{
		u64_to_fd.rehash(100007);
		fd_to_u64.rehash(100007);
		fd_last_active_time.rehash(100007);
	}
	void clear()
	{
		if(disable_conn_clear) return ;

		for(auto it=fd_to_u64.begin();it!=fd_to_u64.end();it++)
		{
			//int fd=int((it->second<<32u)>>32u);
			close(  it->first);
		}
		u64_to_fd.clear();
		fd_to_u64.clear();
		fd_last_active_time.clear();

		clear_it=fd_last_active_time.begin();

	}
	int exist_fd(u32_t fd)
	{
		return fd_to_u64.find(fd)!=fd_to_u64.end();
	}
	int exist_u64(u64_t u64)
	{
		return u64_to_fd.find(u64)!=u64_to_fd.end();
	}
	u32_t find_fd_by_u64(u64_t u64)
	{
		return u64_to_fd[u64];
	}
	u64_t find_u64_by_fd(u32_t fd)
	{
		return fd_to_u64[fd];
	}
	int update_active_time(u32_t fd)
	{
		return fd_last_active_time[fd]=get_current_time();
	}
	int insert_fd(u32_t fd,u64_t u64)
	{
		int before=fd_last_active_time.bucket_count();
		u64_to_fd[u64]=fd;
		fd_to_u64[fd]=u64;
		fd_last_active_time[fd]=get_current_time();
		int after=fd_last_active_time.bucket_count();
		if(after!=before)//rehash happens!
		{
			clear_it=fd_last_active_time.begin();
		}
		return 0;
	}
	int erase(unordered_map<u32_t,u64_t>::iterator it)
	{
		if(disable_conn_clear) return 0;
		if(clear_it==it)
			clear_it++;

		int fd=it->first;
		u64_t u64=fd_to_u64[fd];

		u32_t ip= (u64 >> 32u);

		int port= uint16_t((u64 << 32u) >> 32u);

		mylog(log_info,"[udp] inactive connection [%s:%d] cleared\n",my_ntoa(ip),port);

		fd_manager.fd_close(fd);

		fd_to_u64.erase(fd);
		u64_to_fd.erase(u64);
		fd_last_active_time.erase(fd);
		return 0;
	}
	int erase_fd(int fd)
	{
		auto it=fd_last_active_time.find(fd);
		assert(it!=fd_last_active_time.end());
		erase(it);
		return 0;
	}
	int clear_inactive()
	{
		if(get_current_time()-last_clear_time>conn_clear_interval)
		{
			last_clear_time=get_current_time();
			return clear_inactive0();
		}
		return 0;
	}
	int clear_inactive0()
	{
		if(disable_conn_clear) return 0;

		unordered_map<u32_t,u64_t>::iterator it,old_it;
		int cnt=0;
		it=clear_it;
		int size=fd_last_active_time.size();
		int num_to_clean=size/conn_clear_ratio+conn_clear_min;   //clear 1/10 each time,to avoid latency glitch

		u64_t current_time=get_current_time();
		for(;;)
		{
			if(cnt>=num_to_clean) break;
			if(fd_last_active_time.begin()==fd_last_active_time.end()) break;

			if(it==fd_last_active_time.end())
			{
				it=fd_last_active_time.begin();
			}

			if( current_time -it->second  >conn_timeout_udp )
			{
				//mylog(log_info,"inactive conv %u cleared \n",it->first);
				old_it=it;
				it++;
				//u32_t fd= old_it->first;
				erase(old_it);

			}
			else
			{
				it++;
			}
			cnt++;
		}
		return 0;
	}
}conn_manager;


int sendto_u64 (int fd,char * buf, int len,int flags, u64_t u64)
{

	sockaddr_in tmp_sockaddr;

	memset(&tmp_sockaddr,0,sizeof(tmp_sockaddr));
	tmp_sockaddr.sin_family = AF_INET;
	tmp_sockaddr.sin_addr.s_addr = (u64 >> 32u);

	tmp_sockaddr.sin_port = htons(uint16_t((u64 << 32u) >> 32u));

	return sendto(fd, buf,
			len , 0,
			(struct sockaddr *) &tmp_sockaddr,
			sizeof(tmp_sockaddr));
}

int send_fd (int fd,char * buf, int len,int flags)
{
	return send(fd,buf,len,flags);
}

int create_new_udp(int &new_udp_fd,u32_t ip,int port)
{
	struct sockaddr_in remote_addr_in;

	socklen_t slen = sizeof(sockaddr_in);
	memset(&remote_addr_in, 0, sizeof(remote_addr_in));
	remote_addr_in.sin_family = AF_INET;
	remote_addr_in.sin_port = htons(port);
	remote_addr_in.sin_addr.s_addr = ip;

	new_udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (new_udp_fd < 0) {
		mylog(log_warn, "create udp_fd error\n");
		return -1;
	}
	setnonblocking(new_udp_fd);
	set_buf_size(new_udp_fd,socket_buf_size);

	mylog(log_debug, "created new udp_fd %d\n", new_udp_fd);
	int ret = connect(new_udp_fd, (struct sockaddr *) &remote_addr_in, slen);
	if (ret != 0) {
		mylog(log_warn, "udp fd connect fail %d %s\n",ret,strerror(errno));
		close(new_udp_fd);
		return -1;
	}


	return 0;
}
int set_timer(int epollfd,int &timer_fd)
{
	int ret;
	epoll_event ev;

	itimerspec its;
	memset(&its,0,sizeof(its));

	if((timer_fd=timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK)) < 0)
	{
		mylog(log_fatal,"timer_fd create error\n");
		myexit(1);
	}
	its.it_interval.tv_sec=(timer_interval/1000);
	its.it_interval.tv_nsec=(timer_interval%1000)*1000ll*1000ll;
	its.it_value.tv_nsec=1; //imidiately
	timerfd_settime(timer_fd,0,&its,0);


	ev.events = EPOLLIN;
	ev.data.u64 = timer_fd;

	ret=epoll_ctl(epollfd, EPOLL_CTL_ADD, timer_fd, &ev);
	if (ret < 0) {
		mylog(log_fatal,"epoll_ctl return %d\n", ret);
		myexit(-1);
	}
	return 0;
}

struct conn_manager_tcp_t
{
	list<tcp_pair_t> tcp_pair_list;
	long long last_clear_time;
	list<tcp_pair_t>::iterator clear_it;
	conn_manager_tcp_t()
	{
		clear_it=tcp_pair_list.begin();
	}
	int erase(list<tcp_pair_t>::iterator &it)
	{
		if(clear_it==it)
		{
			clear_it++;
		}
		fd_manager.fd64_close( it->local.fd64);
		fd_manager.fd64_close( it->remote.fd64);
		tcp_pair_list.erase(it);
		return 0;
	}
	int clear_inactive()
	{
		if(get_current_time()-last_clear_time>conn_clear_interval)
		{
			last_clear_time=get_current_time();
			return clear_inactive0();
		}
		return 0;
	}
	int clear_inactive0()
	{
		if(disable_conn_clear) return 0;

		int cnt=0;
		list<tcp_pair_t>::iterator it=clear_it,old_it;
		int size=tcp_pair_list.size();
		int num_to_clean=size/conn_clear_ratio+conn_clear_min;   //clear 1/10 each time,to avoid latency glitch

		u64_t current_time=get_current_time();
		for(;;)
		{
			if(cnt>=num_to_clean) break;
			if(tcp_pair_list.begin()==tcp_pair_list.end()) break;

			if(it==tcp_pair_list.end())
			{
				it=tcp_pair_list.begin();
			}

			if( current_time - it->last_active_time  >conn_timeout_tcp)
			{
				mylog(log_info,"[tcp]inactive connection [%s] cleared \n",it->ip_port_s);
				old_it=it;
				it++;
				//u32_t fd= old_it->first;
				erase(old_it);
			}
			else
			{
				it++;
			}
			cnt++;
		}
		clear_it=it;
		return 0;
	}
}conn_manager_tcp;
int event_loop()
{
	struct sockaddr_in local_me,remote_dst;	int yes = 1;int ret;
	local_listen_fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if(local_listen_fd_tcp<0)
	{
		mylog(log_fatal,"[tcp]create listen socket failed\n");
		myexit(1);
	}

	setsockopt(local_listen_fd_tcp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); //avoid annoying bind problem
	set_buf_size(local_listen_fd_tcp,socket_buf_size);
	setnonblocking(local_listen_fd_tcp);


	local_listen_fd_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(local_listen_fd_udp<0)
	{
		mylog(log_fatal,"[udp]create listen socket failed\n");
		myexit(1);
	}
	setsockopt(local_listen_fd_udp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	set_buf_size(local_listen_fd_udp,4*1024*1024);
	setnonblocking(local_listen_fd_udp);



	socklen_t local_len = sizeof(sockaddr_in);
	memset(&local_me, 0, sizeof(local_me));
	local_me.sin_family = AF_INET;
	local_me.sin_port = htons(local_port);
	local_me.sin_addr.s_addr = local_address_u32;


	int epollfd = epoll_create1(0);
	const int max_events = 4096;
	struct epoll_event ev, events[max_events];
	if (epollfd < 0)
	{
		mylog(log_fatal,"epoll created return %d\n", epollfd);
		myexit(-1);
	}
	if(enable_tcp)
	{
		if (bind(local_listen_fd_tcp, (struct sockaddr*) &local_me, local_len) !=0)
		{
			mylog(log_fatal,"[tcp]socket bind failed, %s",strerror(errno));
			myexit(1);
		}

	    if (listen (local_listen_fd_tcp, 512) !=0) //512 is max pending tcp connection
	    {
			mylog(log_fatal,"[tcp]socket listen failed error, %s",strerror(errno));
			myexit(1);
	    }

		ev.events = EPOLLIN;
		ev.data.u64 = local_listen_fd_tcp;
		int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, local_listen_fd_tcp, &ev);
		if(ret!=0)
		{
			mylog(log_fatal,"[tcp]epoll EPOLL_CTL_ADD return %d\n", epollfd);
			myexit(-1);
		}
	}

	if(enable_udp)
	{
		if (bind(local_listen_fd_udp, (struct sockaddr*) &local_me, local_len) == -1)
		{
			mylog(log_fatal,"[udp]socket bind error");
			myexit(1);
		}

		ev.events = EPOLLIN;
		ev.data.u64 = local_listen_fd_udp;
		int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, local_listen_fd_udp, &ev);
		if(ret!=0)
		{
			mylog(log_fatal,"[udp]epoll created return %d\n", epollfd);
			myexit(-1);
		}
	}

	int clear_timer_fd=-1;
	set_timer(epollfd,clear_timer_fd);

	socklen_t remote_len = sizeof(sockaddr_in);
	memset(&remote_dst, 0, sizeof(remote_dst));
	remote_dst.sin_family = AF_INET;
	remote_dst.sin_port = htons(remote_port);
	remote_dst.sin_addr.s_addr = remote_address_u32;

	u32_t roller=0;
	for (;;)
	{
		int nfds = epoll_wait(epollfd, events, max_events, 180 * 1000); //3mins
		if (nfds < 0)
		{
			mylog(log_fatal,"epoll_wait return %d\n", nfds);
			myexit(-1);
		}
		int idx;
		for (idx = 0; idx < nfds; idx++)
		{
			if(events[idx].data.u64==(u64_t)local_listen_fd_tcp)
			{
				if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
				{
					mylog(log_error,"[tcp]EPOLLERR or EPOLLHUP from listen_fd events[idx].events=%x \n",events[idx].events);
					//continue;
				}

				socklen_t tmp_len = sizeof(sockaddr_in);
				struct sockaddr_in addr_tmp;
				memset(&addr_tmp,0,sizeof(addr_tmp));
				int new_fd=accept(local_listen_fd_tcp, (struct sockaddr*) &addr_tmp,&tmp_len);
				if(new_fd<0)
				{
					mylog(log_warn,"[tcp]accept failed %d %s\n", new_fd,strerror(errno));
					continue;
				}
				char ip_port_s[30];
				sprintf(ip_port_s,"%s:%d",my_ntoa(addr_tmp.sin_addr.s_addr),addr_tmp.sin_port);

				if(int(conn_manager_tcp.tcp_pair_list.size())>=max_conn_num)
				{
					mylog(log_warn,"[tcp]new connection from [%s],but ignored,bc of max_conn_num reached\n",ip_port_s);
					close(new_fd);
					continue;
				}


				int new_remote_fd = socket(AF_INET, SOCK_STREAM, 0);
				if(new_remote_fd<0)
				{
					mylog(log_fatal,"[tcp]create new_remote_fd failed \n");
					myexit(1);
				}
				set_buf_size(new_remote_fd,socket_buf_size);
				setnonblocking(new_remote_fd);

				ret=connect(new_remote_fd,(struct sockaddr*) &remote_dst,remote_len);
				if(ret!=0)
				{
					mylog(log_debug,"[tcp]connect returned %d,errno=%s\n",ret,strerror(errno));
				}
				else
				{
					mylog(log_debug,"[tcp]connect returned 0\n");
				}

				conn_manager_tcp.tcp_pair_list.emplace_back();
				auto it=conn_manager_tcp.tcp_pair_list.end();
				it--;
				tcp_pair_t &tcp_pair=*it;
				strcpy(tcp_pair.ip_port_s,ip_port_s);

				mylog(log_info,"[tcp]new_connection from [%s]\n",tcp_pair.ip_port_s);

				tcp_pair.local.fd64=fd_manager.create(new_fd);
				fd_manager.get_info(tcp_pair.local.fd64).tcp_pair_p= &tcp_pair;
				fd_manager.get_info(tcp_pair.local.fd64).is_tcp=1;
				tcp_pair.local.ev.events=EPOLLIN;
				tcp_pair.local.ev.data.u64=tcp_pair.local.fd64;

				tcp_pair.remote.fd64=fd_manager.create(new_remote_fd);
				fd_manager.get_info(tcp_pair.remote.fd64).tcp_pair_p= &tcp_pair;
				fd_manager.get_info(tcp_pair.remote.fd64).is_tcp=1;
				tcp_pair.remote.ev.events=EPOLLIN;
				tcp_pair.remote.ev.data.u64=tcp_pair.remote.fd64;

				tcp_pair.last_active_time=get_current_time();
				tcp_pair.it=it;

				epoll_event ev;

				ev=tcp_pair.local.ev;
				ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev);
				assert(ret==0);

				ev=tcp_pair.remote.ev;
				ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, new_remote_fd, &ev);
				assert(ret==0);
			}
			else if (events[idx].data.u64 == (u64_t)local_listen_fd_udp) //data income from local end
			{
				if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
				{
					mylog(log_error,"[udp]EPOLLERR or EPOLLHUP from listen_fd events[idx].events=%x \n",events[idx].events);
					//continue;
				}

				char data[buf_len];
				int data_len;
				socklen_t tmp_len = sizeof(sockaddr_in);
				struct sockaddr_in addr_tmp;
				memset(&addr_tmp,0,sizeof(addr_tmp));

				if ((data_len = recvfrom(local_listen_fd_udp, data, max_data_len, 0,
						(struct sockaddr *) &addr_tmp, &tmp_len)) == -1) //<--first packet from a new ip:port turple
				{
					mylog(log_error,"[udp]recv_from error,errno %s,this shouldnt happen,but lets try to pretend it didnt happen",strerror(errno));
					//myexit(1);
					continue;
				}


				data[data_len] = 0; //for easier debug

				ip_port_t ip_port;
				ip_port.ip=addr_tmp.sin_addr.s_addr;
				ip_port.port=ntohs(addr_tmp.sin_port);
				u64_t u64=ip_port.to_u64();
				mylog(log_trace, "[udp]received data from udp_listen_fd from [%s], len=%d\n",ip_port.to_s(),data_len);

				if(!conn_manager.exist_u64(u64))
				{

					if(int(conn_manager.fd_to_u64.size())>=max_conn_num)
					{
						mylog(log_info,"[udp]new connection from [%s],but ignored,bc of max_conv_num reached\n",ip_port.to_s());
						continue;
					}
					int new_udp_fd;
					if(create_new_udp(new_udp_fd,remote_address_u32,remote_port)!=0)
					{
						mylog(log_info,"[udp]new connection from [%s] ,but create udp fd failed\n",ip_port.to_s());
						continue;
					}
					fd64_t fd64=fd_manager.create(new_udp_fd);
					fd_manager.get_info(fd64);//just create the info

					struct epoll_event ev;
					mylog(log_trace, "[udp]u64: %lld\n", u64);
					ev.events = EPOLLIN;
					ev.data.u64 = fd64;

					ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, new_udp_fd, &ev);
					assert(ret==0);
					mylog(log_info,"[udp]new connection from [%s] ,created new udp fd %d\n",ip_port.to_s(),new_udp_fd);
					conn_manager.insert_fd(new_udp_fd,u64);
				}
				int new_udp_fd=conn_manager.find_fd_by_u64(u64);
				conn_manager.update_active_time(new_udp_fd);
				int ret;

				ret = send_fd(new_udp_fd, data,data_len, 0);
				if (ret < 0) {
					mylog(log_warn, "[udp]send returned %d,%s\n", ret,strerror(errno));
				}

			}
			else if(events[idx].data.u64 == (u64_t)clear_timer_fd)
			{
				u64_t value;
				read(clear_timer_fd, &value, 8);

				if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
				{
					mylog(log_warn,"EPOLLERR or EPOLLHUP from clear_timer_fd events[idx].events=%x \n",events[idx].events);
					continue;
				}

				mylog(log_trace, "timer!\n");
				roller++;
				roller&=0x0001;
				if(roller==0) conn_manager.clear_inactive();
				else if(roller==1) conn_manager_tcp.clear_inactive();

			}
			else if(events[idx].data.u64 > u32_t(-1))
			{
				fd64_t fd64=events[idx].data.u64;
				if(!fd_manager.exist(fd64))
				{
					mylog(log_trace,"[tcp]fd64 no longer exist\n");
					continue;
				}
				assert(fd_manager.exist_info(fd64));
				fd_info_t & fd_info=fd_manager.get_info(fd64);
				if(fd_info.is_tcp==1)
				{
					tcp_pair_t &tcp_pair=*(fd_info.tcp_pair_p);

					if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
					{
						mylog(log_info,"[tcp]connection closed, events[idx].events=%x \n",events[idx].events);
						conn_manager_tcp.erase(tcp_pair.it);
						continue;
					}

					tcp_info_t *my_info_p,*other_info_p;
					if(fd64==tcp_pair.local.fd64)
					{
						mylog(log_trace,"[tcp]fd64==tcp_pair.local.fd64\n");
						my_info_p=&tcp_pair.local;
						other_info_p=&tcp_pair.remote;
					}
					else if(fd64==tcp_pair.remote.fd64)
					{
						mylog(log_trace,"[tcp]fd64==tcp_pair.remote.fd64\n");
						my_info_p=&tcp_pair.remote;
						other_info_p=&tcp_pair.local;
					}
					else
					{
						assert(0==1);
					}
					tcp_info_t &my_info=*my_info_p;
					tcp_info_t &other_info=*other_info_p;

					int my_fd=fd_manager.to_fd(my_info.fd64);
					int other_fd=fd_manager.to_fd(other_info.fd64);


					if( (events[idx].events & EPOLLIN) !=0  )
					{
						mylog(log_trace,"[tcp]events[idx].events & EPOLLIN !=0 ,idx =%d\n",idx);
						if((my_info.ev.events&EPOLLIN) ==0)
						{
							mylog(log_debug,"[tcp]out of date event, my_info.ev.events&EPOLLIN) ==0 \n");
							continue;
						}
						assert(my_info.data_len==0);
						int recv_len=recv(my_fd,my_info.data,max_data_len*5,0);//use a larger buffer than udp
						mylog(log_trace,"fd=%d,recv_len=%d\n",my_fd,recv_len);
						if(recv_len==0)
						{
							mylog(log_info,"[tcp]recv_len=%d,connection [%s] closed bc of EOF\n",recv_len,tcp_pair.ip_port_s);
							conn_manager_tcp.erase(tcp_pair.it);
							continue;
						}
						if(recv_len<0)
						{
							mylog(log_info,"[tcp]recv_len=%d,connection [%s] closed bc of %s\n",recv_len,tcp_pair.ip_port_s,strerror(errno));
							conn_manager_tcp.erase(tcp_pair.it);
							continue;
						}
						tcp_pair.last_active_time=get_current_time();

						epoll_event ev;

						assert((other_info.ev.events & EPOLLOUT)==0);
						other_info.ev.events|=EPOLLOUT;
						ev=other_info.ev;
						ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, other_fd, &ev);
						assert(ret==0);

						my_info.ev.events&=~EPOLLIN;
						ev=my_info.ev;
						ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, my_fd, &ev);
						assert(ret==0);

						my_info.data_len=recv_len;
						my_info.begin=my_info.data;
					}
					else if( (events[idx].events & EPOLLOUT) !=0)
					{
						mylog(log_trace,"[tcp]events[idx].events & EPOLLOUT !=0 ,idx =%d\n",idx);

						if( (my_info.ev.events&EPOLLOUT) ==0)
						{
							mylog(log_debug,"[tcp]out of date event, my_info.ev.events&EPOLLOUT) ==0 \n");
							continue;
						}

						assert(other_info.data_len!=0);
						int send_len=send(my_fd,other_info.begin,other_info.data_len,MSG_NOSIGNAL);
						if(send_len==0)
						{
							mylog(log_warn,"[tcp]send_len=%d,connection [%s] closed bc of send_len==0\n",send_len,tcp_pair.ip_port_s);
							conn_manager_tcp.erase(tcp_pair.it);
							continue;
						}
						if(send_len<0)
						{
							mylog(log_info,"[tcp]send_len=%d,connection [%s] closed bc of %s\n",send_len,tcp_pair.ip_port_s,strerror(errno));
							conn_manager_tcp.erase(tcp_pair.it);
							continue;
						}
						tcp_pair.last_active_time=get_current_time();

						mylog(log_trace,"[tcp]fd=%d send len=%d\n",my_fd,send_len);
						other_info.data_len-=send_len;
						other_info.begin+=send_len;

						if(other_info.data_len==0)
						{
							my_info.ev.events&=~EPOLLOUT;
							ev=my_info.ev;
							ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, my_fd, &ev);
							assert(ret==0);

							assert((other_info.ev.events & EPOLLIN)==0);
							other_info.ev.events|=EPOLLIN;
							ev=other_info.ev;
							ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, other_fd, &ev);
							assert(ret==0);
						}
						else
						{
							//keep waitting for EPOLLOUT;
						}
					}
					else
					{
						mylog(log_fatal,"[tcp]got unexpected event,events[idx].events=%x\n",events[idx].events);
						myexit(-1);
					}
				}
				else  //its a udp connection
				{

					int udp_fd=fd_manager.to_fd(fd64);
					assert(conn_manager.exist_fd(udp_fd));
					//if(!conn_manager.exist_fd(udp_fd)) continue;

					if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
					{
						mylog(log_warn,"[udp]EPOLLERR or EPOLLHUP from udp_remote_fd events[idx].events=%x \n",events[idx].events);
						//conn_manager.erase_fd(udp_fd);
						//continue;
					}

					char data[buf_len];
					int data_len =recv(udp_fd,data,max_data_len,0);
					mylog(log_trace, "[udp]received data from udp fd %d, len=%d\n", udp_fd,data_len);
					if(data_len<0)
					{
						if(errno==ECONNREFUSED)
						{
							//conn_manager.clear_list.push_back(udp_fd);
							mylog(log_debug, "recv failed %d ,udp_fd%d,errno:%s\n", data_len,udp_fd,strerror(errno));
						}

						mylog(log_warn,"[udp]recv failed %d ,udp_fd%d,errno:%s\n", data_len,udp_fd,strerror(errno));
						continue;
					}

					assert(conn_manager.exist_fd(udp_fd));

					conn_manager.update_active_time(udp_fd);

					u64_t u64=conn_manager.find_u64_by_fd(udp_fd);

					ret = sendto_u64(local_listen_fd_udp, data,data_len , 0,u64);
					if (ret < 0) {
						mylog(log_warn, "[udp]sento returned %d,%s\n", ret,strerror(errno));
						//perror("ret<0");
					}
				}
			}
			else
			{
				mylog(log_fatal,"got unexpected fd\n");
				myexit(-1);
			}
		}
	}

	myexit(0);
	return 0;
}
void print_help()
{
	char git_version_buf[100]={0};
	strncpy(git_version_buf,gitversion,10);

	printf("tinyForwarder\n");
	printf("git version:%s    ",git_version_buf);
	printf("build date:%s %s\n",__DATE__,__TIME__);
	printf("repository: https://github.com/wangyu-/tinyForwarder\n");
	printf("\n");
	printf("usage ./this_program -c -l local_listen_ip:local_port -r server_ip:server_port  [options]\n");
	printf("    run as server : ./this_program -s -l server_listen_ip:server_port -r remote_ip:remote_port  [options]\n");
	printf("\n");

	printf("options:\n");
	printf("    -t                                    enable tcp forward\n");
	printf("    -u                                    enable udp forward\n");
	printf("    -h,--help                             print this help message\n");
	printf("\n");
	printf("NOTE: If no option is provided,this program enables both tcp and udp forward\n");

	//printf("common options,these options must be same on both side\n");
}
void process_arg(int argc, char *argv[])
{
	int i, j, k;
	int opt;
    static struct option long_options[] =
      {
		{"log-level", required_argument,    0, 1},
		{"log-position", no_argument,    0, 1},
		{"disable-color", no_argument,    0, 1},
		{"disable-filter", no_argument,    0, 1},
		{"sock-buf", required_argument,    0, 1},
		{"random-drop", required_argument,    0, 1},
		{"report", required_argument,    0, 1},
		{NULL, 0, 0, 0}
      };
    int option_index = 0;
	if (argc == 1)
	{
		print_help();
		myexit( -1);
	}
	for (i = 0; i < argc; i++)
	{
		if(strcmp(argv[i],"-h")==0||strcmp(argv[i],"--help")==0)
		{
			print_help();
			myexit(0);
		}
	}
	for (i = 0; i < argc; i++)
	{
		if(strcmp(argv[i],"--log-level")==0)
		{
			if(i<argc -1)
			{
				sscanf(argv[i+1],"%d",&log_level);
				if(0<=log_level&&log_level<log_end)
				{
				}
				else
				{
					log_bare(log_fatal,"invalid log_level\n");
					myexit(-1);
				}
			}
		}
		if(strcmp(argv[i],"--disable-color")==0)
		{
			enable_log_color=0;
		}
	}

    mylog(log_info,"argc=%d ", argc);

	for (i = 0; i < argc; i++) {
		log_bare(log_info, "%s ", argv[i]);
	}
	log_bare(log_info, "\n");

	if (argc == 1)
	{
		print_help();
		myexit(-1);
	}

	int no_l = 1, no_r = 1;
	while ((opt = getopt_long(argc, argv, "l:r:d:tuhcspk:j:m:",long_options,&option_index)) != -1)
	{
		//string opt_key;
		//opt_key+=opt;
		switch (opt)
		{


		case 'm':
			sscanf(optarg,"%d\n",&max_pending_packet);
			if(max_pending_packet<1000)
			{
				mylog(log_fatal,"max_pending_packet must be >1000\n");
				myexit(-1);
			}
			break;
		case 'l':
			no_l = 0;
			if (strchr(optarg, ':') != 0)
			{
				sscanf(optarg, "%[^:]:%d", local_address, &local_port);
			}
			else
			{
				mylog(log_fatal," -r ip:port\n");
				myexit(1);
				strcpy(local_address, "127.0.0.1");
				sscanf(optarg, "%d", &local_port);
			}
			break;
		case 'r':
			no_r = 0;
			if (strchr(optarg, ':') != 0)
			{
				//printf("in :\n");
				//printf("%s\n",optarg);
				sscanf(optarg, "%[^:]:%d", remote_address, &remote_port);
				//printf("%d\n",remote_port);
			}
			else
			{
				mylog(log_fatal," -r ip:port\n");
				myexit(1);
				strcpy(remote_address, "127.0.0.1");
				sscanf(optarg, "%d", &remote_port);
			}
			break;
		case 't':
			enable_tcp=1;
			break;
		case 'u':
			enable_udp=1;
			break;
		case 'h':
			break;
		case 1:
			if(strcmp(long_options[option_index].name,"log-level")==0)
			{
			}
			else if(strcmp(long_options[option_index].name,"disable-color")==0)
			{
				//enable_log_color=0;
			}
			else if(strcmp(long_options[option_index].name,"log-position")==0)
			{
				enable_log_position=1;
			}
			else if(strcmp(long_options[option_index].name,"sock-buf")==0)
			{
				int tmp=-1;
				sscanf(optarg,"%d",&tmp);
				if(10<=tmp&&tmp<=10*1024)
				{
					socket_buf_size=tmp*1024;
				}
				else
				{
					mylog(log_fatal,"sock-buf value must be between 1 and 10240 (kbyte) \n");
					myexit(-1);
				}
			}
			else
			{
				mylog(log_fatal,"unknown option\n");
				myexit(-1);
			}
			break;
		default:
			mylog(log_fatal,"unknown option <%x>", opt);
			myexit(-1);
		}
	}

	if (no_l)
		mylog(log_fatal,"error: -l not found\n");
	if (no_r)
		mylog(log_fatal,"error: -r not found\n");
	if (no_l || no_r)
		myexit(-1);

	if(enable_tcp==0&&enable_udp==0)
	{
		enable_tcp=1;
		enable_udp=1;
	}
}
int main(int argc, char *argv[])
{

	//signal(SIGPIPE, SIG_IGN);

	assert(sizeof(u64_t)==8);
	assert(sizeof(i64_t)==8);
	assert(sizeof(u32_t)==4);
	assert(sizeof(i32_t)==4);
	dup2(1, 2);		//redirect stderr to stdout
	int i, j, k;
	process_arg(argc,argv);
	//init_random_number_fd();

	remote_address_u32=inet_addr(remote_address);
	local_address_u32=inet_addr(local_address);

	//udp_event_loop();

	event_loop();


	return 0;
}

