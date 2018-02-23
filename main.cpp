#include "common.h"
#include "log.h"
#include "git_version.h"
#include "fd_manager.h"

using  namespace std;

typedef unsigned long long u64_t;   //this works on most platform,avoid using the PRId64
typedef long long i64_t;

typedef unsigned int u32_t;
typedef int i32_t;

int disable_conn_clear=0;

int max_pending_packet=0;
int enable_udp=0,enable_tcp=0;

const int listen_fd_buf_size=5*1024*1024;
address_t local_addr,remote_addr;

int VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV;


struct conn_manager_udp_t
{
	unordered_map<address_t,udp_pair_t*,address_t::hash_function> adress_to_info;

	list<udp_pair_t> udp_pair_list;
	long long last_clear_time;
	list<udp_pair_t>::iterator clear_it;

	conn_manager_udp_t()
	{
		last_clear_time=0;
		adress_to_info.reserve(10007);
		clear_it=udp_pair_list.begin();
	}

	int erase(list<udp_pair_t>::iterator &it)
	{
		mylog(log_info,"[udp]inactive connection {%s} cleared, udp connections=%d\n",it->addr_s,(int)udp_pair_list.size());

		auto tmp_it=adress_to_info.find(it->adress);
		assert(tmp_it!=adress_to_info.end());
		adress_to_info.erase(tmp_it);

		fd_manager.fd64_close(it->fd64);
		udp_pair_list.erase(it);

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
		list<udp_pair_t>::iterator it=clear_it,old_it;
		int size=udp_pair_list.size();
		int num_to_clean=size/conn_clear_ratio+conn_clear_min;   //clear 2% each time,to avoid latency glitch

		u64_t current_time=get_current_time();
		num_to_clean=min(num_to_clean,size);
		for(;;)
		{
			if(cnt>=num_to_clean) break;
			if(udp_pair_list.begin()==udp_pair_list.end()) break;

			if(it==udp_pair_list.end())
			{
				it=udp_pair_list.begin();
			}

			if( current_time - it->last_active_time  >conn_timeout_udp)
			{
				old_it=it;
				it++;
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
}conn_manager_udp;

struct conn_manager_tcp_t
{
	list<tcp_pair_t> tcp_pair_list;
	long long last_clear_time;
	list<tcp_pair_t>::iterator clear_it;
	conn_manager_tcp_t()
	{
		last_clear_time=0;
		clear_it=tcp_pair_list.begin();
	}
	int delayed_erase(list<tcp_pair_t>::iterator &it)
	{
		fd_manager.fd64_close( it->local.fd64);
		fd_manager.fd64_close( it->remote.fd64);
		it->not_used=1;
		it->local.free_memory();
		it->remote.free_memory();
		return 0;
	}
	int erase(list<tcp_pair_t>::iterator &it)
	{
		/*if(clear_it==it)
		{
			clear_it++;
		}*/
		if(!it->not_used)
		{
			fd_manager.fd64_close( it->local.fd64);
			fd_manager.fd64_close( it->remote.fd64);
			mylog(log_info,"[tcp]inactive connection {%s} cleared, tcp connections=%d\n",it->addr_s,(int)tcp_pair_list.size());
		}
		else
		{
			mylog(log_info,"[tcp]closed connection {%s} cleared, tcp connections=%d\n",it->addr_s,(int)tcp_pair_list.size());
		}
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
		int num_to_clean=size/conn_clear_ratio+conn_clear_min;   //clear 2% each time,to avoid latency glitch

		u64_t current_time=get_current_time();
		num_to_clean=min(num_to_clean,size);
		for(;;)
		{
			if(cnt>=num_to_clean) break;
			if(tcp_pair_list.begin()==tcp_pair_list.end()) break;

			if(it==tcp_pair_list.end())
			{
				it=tcp_pair_list.begin();
			}

			if( it->not_used||current_time - it->last_active_time  >conn_timeout_tcp)
			{
				old_it=it;
				it++;
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
	int local_listen_fd_tcp=-1;
	int local_listen_fd_udp=-1;

	//struct sockaddr_in local_me,remote_dst;
	int yes = 1;int ret;
	local_listen_fd_tcp = socket(local_addr.get_type(), SOCK_STREAM, 0);
	if(local_listen_fd_tcp<0)
	{
		mylog(log_fatal,"[tcp]create listen socket failed\n");
		myexit(1);
	}

	setsockopt(local_listen_fd_tcp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); //avoid annoying bind problem
	set_buf_size(local_listen_fd_tcp,listen_fd_buf_size);
	setnonblocking(local_listen_fd_tcp);


	local_listen_fd_udp = socket(local_addr.get_type(), SOCK_DGRAM, IPPROTO_UDP);
	if(local_listen_fd_udp<0)
	{
		mylog(log_fatal,"[udp]create listen socket failed\n");
		myexit(1);
	}
	setsockopt(local_listen_fd_udp, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));  //this is not necessary.
	set_buf_size(local_listen_fd_udp,listen_fd_buf_size);
	setnonblocking(local_listen_fd_udp);



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
		if (bind(local_listen_fd_tcp, (struct sockaddr*) &local_addr.inner, local_addr.get_len()) !=0)
		{
			mylog(log_fatal,"[tcp]socket bind failed, %s",strerror(errno));
			myexit(1);
		}

	    if (listen (local_listen_fd_tcp, 512) !=0) //512 is max pending tcp connection,its large enough
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
		if (bind(local_listen_fd_udp, (struct sockaddr*) &local_addr.inner, local_addr.get_len()) == -1)
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

	//u32_t roller=0;
	for (;;)
	{
		int nfds = epoll_wait(epollfd, events, max_events, 180 * 1000); //3mins
		if (nfds < 0)
		{
			if(errno==EINTR  )
			{
				mylog(log_info,"epoll interrupted by signal,continue\n");
				//myexit(0);
			}
			else
			{
				mylog(log_fatal,"epoll_wait return %d,%s\n", nfds,strerror(errno));
				myexit(-1);
			}
		}
		int idx;
		for (idx = 0; idx < nfds; idx++)
		{
			if(events[idx].data.u64==(u64_t)local_listen_fd_tcp)
			{
				if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
				{
					mylog(log_error,"[tcp]EPOLLERR or EPOLLHUP from listen_fd events[idx].events=%x \n",events[idx].events);
					//if there is an error, we will eventually get it at accept()
				}

				socklen_t tmp_len = sizeof(address_t::storage_t);
				address_t::storage_t tmp_sockaddr_in={0};
				memset(&tmp_sockaddr_in,0,sizeof(tmp_sockaddr_in));

				int new_fd=accept(local_listen_fd_tcp, (struct sockaddr*) &tmp_sockaddr_in,&tmp_len);
				if(new_fd<0)
				{
					mylog(log_warn,"[tcp]accept failed %d %s\n", new_fd,strerror(errno));
					continue;
				}

				address_t tmp_addr;
				tmp_addr.from_sockaddr((sockaddr*)&tmp_sockaddr_in,tmp_len);

				set_buf_size(new_fd,socket_buf_size);
				setnonblocking(new_fd);

				char ip_addr[max_addr_len];
				tmp_addr.to_str(ip_addr);
				//sprintf(ip_port_s,"%s:%d",my_ntoa(addr_tmp.sin_addr.s_addr),addr_tmp.sin_port);

				if(int(conn_manager_tcp.tcp_pair_list.size())>=max_conn_num)
				{
					mylog(log_warn,"[tcp]new connection from {%s},but ignored,bc of max_conn_num reached\n",ip_addr);
					close(new_fd);
					continue;
				}


				int new_remote_fd = socket(remote_addr.get_type(), SOCK_STREAM, 0);
				if(new_remote_fd<0)
				{
					mylog(log_fatal,"[tcp]create new_remote_fd failed \n");
					myexit(1);
				}
				set_buf_size(new_remote_fd,socket_buf_size);
				setnonblocking(new_remote_fd);

				ret=connect(new_remote_fd,(struct sockaddr*) &remote_addr.inner,remote_addr.get_len());
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
				strcpy(tcp_pair.addr_s,ip_addr);

				mylog(log_info,"[tcp]new_connection from {%s},fd1=%d,fd2=%d,tcp connections=%d\n",tcp_pair.addr_s,new_fd,new_remote_fd,(int)conn_manager_tcp.tcp_pair_list.size());

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
					//if there is an error, we will eventually get it at recvfrom();
				}

				char data[max_data_len_udp+200];
				int data_len;

				socklen_t tmp_len = sizeof(address_t::storage_t);
				address_t::storage_t tmp_sockaddr_in={0};
				memset(&tmp_sockaddr_in,0,sizeof(tmp_sockaddr_in));

				if ((data_len = recvfrom(local_listen_fd_udp, data, max_data_len_udp+1, 0,
						(struct sockaddr *) &tmp_sockaddr_in, &tmp_len)) == -1) //<--first packet from a new ip:port turple
				{
					mylog(log_error,"[udp]recv_from error,errno %s,this shouldnt happen,but lets try to pretend it didnt happen",strerror(errno));
					//myexit(1);
					continue;
				}

				address_t tmp_addr;
				tmp_addr.from_sockaddr((sockaddr*)&tmp_sockaddr_in,tmp_len);

				data[data_len] = 0; //for easier debug

				char ip_addr[max_addr_len];
				tmp_addr.to_str(ip_addr);

				mylog(log_trace, "[udp]received data from udp_listen_fd from {%s}, len=%d\n",ip_addr,data_len);

				if(data_len==max_data_len_udp+1)
				{
					mylog(log_warn,"huge packet from {%s}, data_len > %d,dropped\n",ip_addr,max_data_len_udp);
					continue;
				}

				auto it=conn_manager_udp.adress_to_info.find(tmp_addr);
				if(it==conn_manager_udp.adress_to_info.end())
				{

					if(int(conn_manager_udp.udp_pair_list.size())>=max_conn_num)
					{
						mylog(log_info,"[udp]new connection from {%s},but ignored,bc of max_conv_num reached\n",ip_addr);
						continue;
					}
					int new_udp_fd=remote_addr.new_connected_udp_fd();
					if(new_udp_fd==-1)
					{
						mylog(log_info,"[udp]new connection from {%s} ,but create udp fd failed\n",ip_addr);
						continue;
					}
					fd64_t fd64=fd_manager.create(new_udp_fd);
					fd_manager.get_info(fd64);//just create the info

					struct epoll_event ev;
					mylog(log_trace, "[udp]u64: %lld\n", fd64);
					ev.events = EPOLLIN;
					ev.data.u64 = fd64;

					ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, new_udp_fd, &ev);
					assert(ret==0);


					conn_manager_udp.udp_pair_list.emplace_back();
					auto list_it=conn_manager_udp.udp_pair_list.end();
					list_it--;
					udp_pair_t &udp_pair=*list_it;

					mylog(log_info,"[udp]new connection from {%s},udp fd=%d,udp connections=%d\n",ip_addr,new_udp_fd,(int)conn_manager_udp.udp_pair_list.size());

					udp_pair.adress=tmp_addr;
					udp_pair.fd64=fd64;
					udp_pair.last_active_time=get_current_time();
					strcpy(udp_pair.addr_s,ip_addr);
					udp_pair.it=list_it;

					fd_manager.get_info(fd64).udp_pair_p=&udp_pair;
					conn_manager_udp.adress_to_info[tmp_addr]=&udp_pair;
					it=conn_manager_udp.adress_to_info.find(tmp_addr);
					//it=adress_to_info.
				}

				//auto it=conn_manager_udp.adress_to_info.find(tmp_addr);
				assert(it!=conn_manager_udp.adress_to_info.end() );

				udp_pair_t &udp_pair=*(it->second);
				int udp_fd= fd_manager.to_fd(udp_pair.fd64);
				udp_pair.last_active_time=get_current_time();

				int ret;
				ret = send(udp_fd, data,data_len, 0);
				if (ret < 0) {
					mylog(log_warn, "[udp]send returned %d,%s\n", ret,strerror(errno));
				}

			}
			else if(events[idx].data.u64 == (u64_t)clear_timer_fd)
			{
				if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
				{
					mylog(log_warn,"EPOLLERR or EPOLLHUP from clear_timer_fd events[idx].events=%x \n",events[idx].events);
					//continue;
				}
				u64_t value;
				read(clear_timer_fd, &value, 8);

				mylog(log_trace, "timer!\n");
				conn_manager_udp.clear_inactive();
				conn_manager_tcp.clear_inactive();

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
						conn_manager_tcp.delayed_erase(tcp_pair.it);
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
						int recv_len=recv(my_fd,my_info.data,max_data_len_tcp,0);//use a larger buffer than udp
						mylog(log_trace,"fd=%d,recv_len=%d\n",my_fd,recv_len);
						if(recv_len==0)
						{
							mylog(log_info,"[tcp]recv_len=%d,connection {%s} closed bc of EOF\n",recv_len,tcp_pair.addr_s);
							conn_manager_tcp.delayed_erase(tcp_pair.it);
							continue;
						}
						if(recv_len<0)
						{
							mylog(log_info,"[tcp]recv_len=%d,connection {%s} closed bc of %s\n",recv_len,tcp_pair.addr_s,strerror(errno));
							conn_manager_tcp.delayed_erase(tcp_pair.it);
							continue;
						}
						tcp_pair.last_active_time=get_current_time();

						my_info.data_len=recv_len;
						my_info.begin=my_info.data;

						assert((other_info.ev.events & EPOLLOUT)==0);

						int send_len=send(other_fd,my_info.begin,my_info.data_len,MSG_NOSIGNAL);

						if(send_len<=0)
						{
							//NOP
						}
						else
						{
							my_info.data_len-=send_len;
							my_info.begin+=send_len;
						}

						if(my_info.data_len!=0)
						{
							epoll_event ev;
							other_info.ev.events|=EPOLLOUT;
							ev=other_info.ev;
							ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, other_fd, &ev);
							assert(ret==0);

							my_info.ev.events&=~EPOLLIN;
							ev=my_info.ev;
							ret = epoll_ctl(epollfd, EPOLL_CTL_MOD, my_fd, &ev);
							assert(ret==0);
						}

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
							mylog(log_warn,"[tcp]send_len=%d,connection {%s} closed bc of send_len==0\n",send_len,tcp_pair.addr_s);
							conn_manager_tcp.delayed_erase(tcp_pair.it);
							continue;
						}
						if(send_len<0)
						{
							mylog(log_info,"[tcp]send_len=%d,connection {%s} closed bc of %s\n",send_len,tcp_pair.addr_s,strerror(errno));
							conn_manager_tcp.delayed_erase(tcp_pair.it);
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
					udp_pair_t & udp_pair=*fd_manager.get_info(fd64).udp_pair_p;
					//assert(conn_manager.exist_fd(udp_fd));
					//if(!conn_manager.exist_fd(udp_fd)) continue;

					if((events[idx].events & EPOLLERR) !=0 ||(events[idx].events & EPOLLHUP) !=0)
					{
						mylog(log_warn,"[udp]EPOLLERR or EPOLLHUP from udp_remote_fd events[idx].events=%x \n",events[idx].events);

					}

					char data[max_data_len_udp+200];
					int data_len =recv(udp_fd,data,max_data_len_udp+1,0);
					mylog(log_trace, "[udp]received data from udp fd %d, len=%d\n", udp_fd,data_len);

					if(data_len==max_data_len_udp+1)
					{
						mylog(log_warn,"huge packet from {%s}, data_len > %d,dropped\n",udp_pair.addr_s,max_data_len_udp);
						continue;
					}

					if(data_len<0)
					{
						mylog(log_warn,"[udp]recv failed %d ,udp_fd%d,errno:%s\n", data_len,udp_fd,strerror(errno));
						continue;
					}

					udp_pair.last_active_time=get_current_time();

					ret = sendto(local_listen_fd_udp, data,data_len,0, (struct sockaddr *)&udp_pair.adress.inner,udp_pair.adress.get_len());
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

	printf("\n");
	printf("tinyPortMapper\n");
	printf("git version:%s    ",git_version_buf);
	printf("build date:%s %s\n",__DATE__,__TIME__);
	printf("repository: https://github.com/wangyu-/tinyPortMapper\n");
	printf("\n");
	printf("usage:\n");
	printf("    ./this_program  -l <listen_ip>:<listen_port> -r <remote_ip>:<remote_port>  [options]\n");
	printf("\n");

	printf("main options:\n");
	printf("    -t                                    enable TCP forwarding/mapping\n");
	printf("    -u                                    enable UDP forwarding/mapping\n");
	//printf("NOTE: If neither of -t or -u is provided,this program enables both TCP and UDP forward\n");
	printf("\n");

	printf("other options:\n");
	printf("    --sock-buf            <number>        buf size for socket, >=10 and <=10240, unit: kbyte, default: 1024\n");
	printf("    --log-level           <number>        0: never    1: fatal   2: error   3: warn \n");
	printf("                                          4: info (default)      5: debug   6: trace\n");
	printf("    --log-position                        enable file name, function name, line number in log\n");
	printf("    --disable-color                       disable log color\n");
	printf("    -h,--help                             print this help message\n");
	printf("\n");


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
		{"sock-buf", required_argument,    0, 1},
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
	while ((opt = getopt_long(argc, argv, "l:r:tuh:",long_options,&option_index)) != -1)
	{
		//string opt_key;
		//opt_key+=opt;
		switch (opt)
		{

		case 'l':
			no_l = 0;
			local_addr.from_str(optarg);
			break;
		case 'r':
			no_r = 0;
			remote_addr.from_str(optarg);
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
		//enable_tcp=1;
		//enable_udp=1;
		mylog(log_fatal,"you must specify -t or -u or both\n");
		myexit(-1);
	}
}

int unit_test()
{
	address_t::hash_function hash;
	address_t test;
	test.from_str((char*)"[2001:19f0:7001:1111:00:ff:11:22]:443");
	printf("%s\n",test.get_str());
	printf("%d\n",hash(test));
	test.from_str((char*)"44.55.66.77:443");
	printf("%s\n",test.get_str());
	printf("%d\n",hash(test));

	return 0;
}
int main(int argc, char *argv[])
{
	//unit_test();
	assert(sizeof(u64_t)==8);
	assert(sizeof(i64_t)==8);
	assert(sizeof(u32_t)==4);
	assert(sizeof(i32_t)==4);
	dup2(1, 2);		//redirect stderr to stdout
	int i, j, k;
	process_arg(argc,argv);

	event_loop();

	return 0;
}

