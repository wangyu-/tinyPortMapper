#include "common.h"
#include "log.h"
#include "git_version.h"

using  namespace std;

typedef unsigned long long u64_t;   //this works on most platform,avoid using the PRId64
typedef long long i64_t;

typedef unsigned int u32_t;
typedef int i32_t;


int random_number_fd=-1;

int remote_fd=-1;
int local_fd=-1;
int is_client = 0, is_server = 0;
int local_listen_fd=-1;

int disable_conn_clear=0;
int mtu_warn=1350;
u32_t remote_address_uint32=0;

char local_address[100], remote_address[100];
int local_port = -1, remote_port = -1;

int max_pending_packet=0;

int VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV;


struct conn_manager_t  //TODO change map to unordered map
{
	//typedef hash_map map;
	unordered_map<u64_t,u32_t> u64_to_fd;  //conv and u64 are both supposed to be uniq
	unordered_map<u32_t,u64_t> fd_to_u64;

	unordered_map<u32_t,u64_t> fd_last_active_time;

	unordered_map<u32_t,u64_t>::iterator clear_it;

	unordered_map<u32_t,u64_t>::iterator it;
	unordered_map<u32_t,u64_t>::iterator old_it;

	//void (*clear_function)(uint64_t u64) ;

	long long last_clear_time;
	list<int> clear_list;
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
		u64_to_fd.rehash(10007);
		fd_to_u64.rehash(10007);
		fd_last_active_time.rehash(10007);
	}
	void clear()
	{
		if(disable_conn_clear) return ;

		for(it=fd_to_u64.begin();it!=fd_to_u64.end();it++)
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
	int erase_fd(u32_t fd)
	{
		if(disable_conn_clear) return 0;
		u64_t u64=fd_to_u64[fd];

		u32_t ip= (u64 >> 32u);

		int port= uint16_t((u64 << 32u) >> 32u);

		mylog(log_info,"fd %d cleared,assocated adress %s,%d\n",fd,my_ntoa(ip),port);

		close(fd);

		fd_to_u64.erase(fd);
		u64_to_fd.erase(u64);
		fd_last_active_time.erase(fd);
		return 0;
	}
	void check_clear_list()
	{
		while(!clear_list.empty())
		{
			int fd=*clear_list.begin();
			clear_list.pop_front();
			erase_fd(fd);
		}
	}
	int clear_inactive()
	{
		if(get_current_time()-last_clear_time>conv_clear_interval)
		{
			last_clear_time=get_current_time();
			return clear_inactive0();
		}
		return 0;
	}
	int clear_inactive0()
	{
		if(disable_conn_clear) return 0;


		//map<uint32_t,uint64_t>::iterator it;
		int cnt=0;
		it=clear_it;
		int size=fd_last_active_time.size();
		int num_to_clean=size/conv_clear_ratio+conv_clear_min;   //clear 1/10 each time,to avoid latency glitch

		u64_t current_time=get_current_time();
		for(;;)
		{
			if(cnt>=num_to_clean) break;
			if(fd_last_active_time.begin()==fd_last_active_time.end()) break;

			if(it==fd_last_active_time.end())
			{
				it=fd_last_active_time.begin();
			}

			if( current_time -it->second  >conv_timeout )
			{
				//mylog(log_info,"inactive conv %u cleared \n",it->first);
				old_it=it;
				it++;
				u32_t fd= old_it->first;
				erase_fd(old_it->first);


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

int create_new_udp(int &new_udp_fd)
{
	struct sockaddr_in remote_addr_in;

	socklen_t slen = sizeof(sockaddr_in);
	memset(&remote_addr_in, 0, sizeof(remote_addr_in));
	remote_addr_in.sin_family = AF_INET;
	remote_addr_in.sin_port = htons(remote_port);
	remote_addr_in.sin_addr.s_addr = remote_address_uint32;

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
	ev.data.fd = timer_fd;

	ret=epoll_ctl(epollfd, EPOLL_CTL_ADD, timer_fd, &ev);
	if (ret < 0) {
		mylog(log_fatal,"epoll_ctl return %d\n", ret);
		myexit(-1);
	}
	return 0;
}
int event_loop()
{
	struct sockaddr_in local_me, local_other;
	local_listen_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int yes = 1;
	//setsockopt(local_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	set_buf_size(local_listen_fd,4*1024*1024);
	setnonblocking(local_listen_fd);

	//char data[buf_len];
	//char *data=data0;
	socklen_t slen = sizeof(sockaddr_in);
	memset(&local_me, 0, sizeof(local_me));
	local_me.sin_family = AF_INET;
	local_me.sin_port = htons(local_port);
	local_me.sin_addr.s_addr = inet_addr(local_address);
	if (bind(local_listen_fd, (struct sockaddr*) &local_me, slen) == -1)
	{
		mylog(log_fatal,"socket bind error");
		myexit(1);
	}

	int epollfd = epoll_create1(0);
	const int max_events = 4096;
	struct epoll_event ev, events[max_events];
	if (epollfd < 0)
	{
		mylog(log_fatal,"epoll created return %d\n", epollfd);
		myexit(-1);
	}
	ev.events = EPOLLIN;
	ev.data.fd = local_listen_fd;
	int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, local_listen_fd, &ev);

	if(ret!=0)
	{
		mylog(log_fatal,"epoll created return %d\n", epollfd);
		myexit(-1);
	}
	int clear_timer_fd=-1;
	set_timer(epollfd,clear_timer_fd);



	for (;;)
	{
		int nfds = epoll_wait(epollfd, events, max_events, 180 * 1000); //3mins
		if (nfds < 0)
		{
			mylog(log_fatal,"epoll_wait return %d\n", nfds);
			myexit(-1);
		}
		int n;
		int clear_triggered=0;
		for (n = 0; n < nfds; ++n)
		{
			if (events[n].data.fd == local_listen_fd) //data income from local end
			{

				char data[buf_len];
				int data_len;

				slen = sizeof(sockaddr_in);
				if ((data_len = recvfrom(local_listen_fd, data, max_data_len, 0,
						(struct sockaddr *) &local_other, &slen)) == -1) //<--first packet from a new ip:port turple
				{

					mylog(log_error,"recv_from error,errno %s,this shouldnt happen,but lets try to pretend it didnt happen",strerror(errno));
					//myexit(1);
					continue;
				}
				mylog(log_trace, "received data from listen fd,%s:%d, len=%d\n", my_ntoa(local_other.sin_addr.s_addr),ntohs(local_other.sin_port),data_len);
				if(data_len>mtu_warn)
				{
					mylog(log_warn,"huge packet,data len=%d (>%d).strongly suggested to set a smaller mtu at upper level,to get rid of this warn\n ",data_len,mtu_warn);
				}
				data[data_len] = 0; //for easier debug
				u64_t u64=pack_u64(local_other.sin_addr.s_addr,ntohs(local_other.sin_port));

				if(!conn_manager.exist_u64(u64))
				{

					if(int(conn_manager.fd_to_u64.size())>=max_conv_num)
					{
						mylog(log_info,"new connection from %s:%d ,but ignored,bc of max_conv_num reached\n",my_ntoa(local_other.sin_addr.s_addr),ntohs(local_other.sin_port));
						continue;
					}
					int new_udp_fd;
					if(create_new_udp(new_udp_fd)!=0)
					{
						mylog(log_info,"new connection from %s:%d ,but create udp fd failed\n",my_ntoa(local_other.sin_addr.s_addr),ntohs(local_other.sin_port));
						continue;
					}
					struct epoll_event ev;

					mylog(log_trace, "u64: %lld\n", u64);
					ev.events = EPOLLIN;

					ev.data.fd = new_udp_fd;

					ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, new_udp_fd, &ev);
					if (ret != 0) {
						mylog(log_info,"new connection from %s:%d ,but add to epoll failed\n",my_ntoa(local_other.sin_addr.s_addr),ntohs(local_other.sin_port));
						close(new_udp_fd);
						continue;
					}
					mylog(log_info,"new connection from %s:%d ,created new udp fd %d\n",my_ntoa(local_other.sin_addr.s_addr),ntohs(local_other.sin_port),new_udp_fd);
					conn_manager.insert_fd(new_udp_fd,u64);
				}
				int new_udp_fd=conn_manager.find_fd_by_u64(u64);
				conn_manager.update_active_time(new_udp_fd);
				int ret;

				ret = send_fd(new_udp_fd, data,data_len, 0);
				if (ret < 0) {
					mylog(log_warn, "send returned %d,%s\n", ret,strerror(errno));
				}

			}
			else if(events[n].data.fd == clear_timer_fd)
			{
				clear_triggered=1;
			}

			else
			{
				int udp_fd=events[n].data.fd;
				if(!conn_manager.exist_fd(udp_fd)) continue;

				char data[buf_len];
				int data_len =recv(udp_fd,data,max_data_len,0);
				mylog(log_trace, "received data from udp fd %d, len=%d\n", udp_fd,data_len);
				if(data_len<0)
				{
					if(errno==ECONNREFUSED)
					{
						//conn_manager.clear_list.push_back(udp_fd);
						mylog(log_debug, "recv failed %d ,udp_fd%d,errno:%s\n", data_len,udp_fd,strerror(errno));
					}

					mylog(log_warn, "recv failed %d ,udp_fd%d,errno:%s\n", data_len,udp_fd,strerror(errno));
					continue;
				}
				if(data_len>mtu_warn)
				{
					mylog(log_warn,"huge packet,data len=%d (>%d).strongly suggested to set a smaller mtu at upper level,to get rid of this warn\n ",data_len,mtu_warn);
				}

				assert(conn_manager.exist_fd(udp_fd));

				conn_manager.update_active_time(udp_fd);

				u64_t u64=conn_manager.find_u64_by_fd(udp_fd);

				ret = sendto_u64(local_listen_fd, data,
						data_len , 0,u64);
				if (ret < 0) {
					mylog(log_warn, "sento returned %d,%s\n", ret,strerror(errno));
					//perror("ret<0");
				}

			}
		}
		if(clear_triggered)   // 删除操作在epoll event的最后进行，防止event cache中的fd失效。
		{
			u64_t value;
			read(clear_timer_fd, &value, 8);
			mylog(log_trace, "timer!\n");
			conn_manager.clear_inactive();
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
	printf("usage:\n");
	printf("    run as client : ./this_program -c -l local_listen_ip:local_port -r server_ip:server_port  [options]\n");
	printf("    run as server : ./this_program -s -l server_listen_ip:server_port -r remote_ip:remote_port  [options]\n");
	printf("\n");
	printf("common option,must be same on both sides:\n");
	printf("    -k,--key              <string>        key for simple xor encryption,default:\"secret key\"\n");

	printf("main options:\n");
	printf("    -d                    <number>        duplicated packet number, -d 0 means no duplicate. default value:0\n");
	printf("    -t                    <number>        duplicated packet delay time, unit: 0.1ms,default value:20(2ms)\n");
	printf("    -j                    <number>        simulated jitter.randomly delay first packet for 0~jitter_value*0.1 ms,to\n");
	printf("                                          create simulated jitter.default value:0.do not use if you dont\n");
	printf("                                          know what it means\n");
	printf("    --report              <number>        turn on udp send/recv report,and set a time interval for reporting,unit:s\n");
	printf("advanced options:\n");
	printf("    -t                    tmin:tmax       simliar to -t above,but delay randomly between tmin and tmax\n");
	printf("    -j                    jmin:jmax       simliar to -j above,but create jitter randomly between jmin and jmax\n");
	printf("    --random-drop         <number>        simulate packet loss ,unit:0.01%%\n");
	printf("    --disable-filter                      disable duplicate packet filter.\n");
	printf("    -m                    <number>        max pending packets,to prevent the program from eating up all your memory,\n");
	printf("                                          default value:0(disabled).\n");
	printf("other options:\n");
	printf("    --log-level           <number>        0:never    1:fatal   2:error   3:warn \n");
	printf("                                          4:info (default)     5:debug   6:trace\n");
	printf("    --log-position                        enable file name,function name,line number in log\n");
	printf("    --disable-color                       disable log color\n");
	printf("    --sock-buf            <number>        buf size for socket,>=10 and <=10240,unit:kbyte,default:1024\n");
	//printf("    -p                                    use multi-process mode instead of epoll.very costly,only for test,do dont use\n");
	printf("    -h,--help                             print this help message\n");

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
	while ((opt = getopt_long(argc, argv, "l:r:d:t:hcspk:j:m:",long_options,&option_index)) != -1)
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

		case 'c':
			is_client = 1;
			break;
		case 's':
			is_server = 1;
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
		mylog(log_fatal,"error: -i not found\n");
	if (no_r)
		mylog(log_fatal,"error: -o not found\n");
	if (no_l || no_r)
		myexit(-1);
}
int main(int argc, char *argv[])
{
	assert(sizeof(u64_t)==8);
	assert(sizeof(i64_t)==8);
	assert(sizeof(u32_t)==4);
	assert(sizeof(i32_t)==4);
	dup2(1, 2);		//redirect stderr to stdout
	int i, j, k;
	process_arg(argc,argv);
	//init_random_number_fd();

	remote_address_uint32=inet_addr(remote_address);

	event_loop();


	return 0;
}

