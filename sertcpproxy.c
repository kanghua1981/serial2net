


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>


//#define LISTEN_PORT 50001
#define LISTEN_PORT 8080
unsigned char ser2net_in[1500] = {0};
unsigned char client_in[1500] = {0};
#define MAX_CLIENT_NUM   20

int clientlasttime[MAX_CLIENT_NUM];
int clientlists[MAX_CLIENT_NUM];


int ser2net_socket = -1;
int client_socket = -1;

int connect_ser2net()
{
	struct sockaddr_in server_addr; 
	ser2net_socket =  socket(AF_INET,SOCK_STREAM,0);
	if (ser2net_socket == -1)
	{
		return -1;
	}
	bzero(&server_addr,sizeof(server_addr)); // 初始化,置0
	server_addr.sin_family=AF_INET;          // IPV4
	server_addr.sin_port=htons(2222);  // (将本机器上的short数据转化为网络上的short数据)端口号
	server_addr.sin_addr.s_addr =inet_addr("127.0.0.1");; // IP地址

	/* 客户程序发起连接请求 */ 
	if(connect(ser2net_socket,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1) 
	{ 
		fprintf(stderr,"Connect Error:%s\a\n",strerror(errno)); 
		return -1;
	} 

	/* 连接成功了 */ 
	printf("connect serial ok\n");
	return 0;
}


int connect_to_server(uint8_t *ipaddr,int port)
{
#define TIME_OUT_TIME  10
	struct sockaddr_in server_addr; 
	int flag = 1;
	fd_set set;
	struct timeval tm;
	int error=-1;
	int ret = -1;
	int options =1;
	int len = 0;
	client_socket =  socket(AF_INET,SOCK_STREAM,0);
	if (client_socket == -1)
	{
		return -1;
	}

	bzero(&server_addr,sizeof(server_addr)); // 初始化,置0
	server_addr.sin_family=AF_INET;          // IPV4
	server_addr.sin_port=htons(port);  // (将本机器上的short数据转化为网络上的short数据)端口号
	server_addr.sin_addr.s_addr =inet_addr(ipaddr);; // IP地址
	
	
	ioctl(client_socket,FIONBIO,&flag);
	/* 客户程序发起连接请求 */ 
	if(connect(client_socket,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1) 
	{ 
		//fprintf(stderr,"Connect Error:%s\a\n",strerror(errno)); 
		//return ret;
		//printf("begin connect\n");
		tm.tv_sec = TIME_OUT_TIME;
		tm.tv_usec = 0;
		FD_ZERO(&set);
		FD_SET(client_socket, &set);
		if( select(client_socket+1,NULL , &set, NULL, &tm) > 0)
		{
			/*
			getsockopt(client_socket, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
    			if(error == 0) 
				ret = 0;
			else
			{
				fprintf(stderr,"111Connect Error:%s\a\n",strerror(errno));
				ret = -1;
			}
			*/
			ret = 0;
		}
		else  
	    {
	    	fprintf(stderr,"Connect Error:%s\a\n",strerror(errno));
	        ret = -1;  
	    }  
	}
	else
	{
		ret = 0;
	}
	flag = 0;
	ioctl(client_socket, FIONBIO, &flag); //设置为阻塞模式

	/* 连接成功了 */ 
	if (ret == 0)
		printf("connect ok\n");
	else
	{
		close(client_socket);
		printf("connect failed\n");
	}
	setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY,(char *) &options, sizeof(options));
	return ret;

}


int write_ser2net_to_allclient(char *buff,int size)
{
	int i;
	
	for(i=0;i<MAX_CLIENT_NUM;i++)
    {
        if (clientlists[i]>0)
        {
            write(clientlists[i],buff,size);
        }
    }
	return 0;
}

int write_ser2net_to_server(char *buff,int size)
{
	int ret = 0;
	ret = write(client_socket,buff,size);
	if (ret < 0)
	{
		return -1;
	}
	return 0;
}

int run_in_client(fd_set readfds,uint8_t *address,int port)
{
	int result = 0;
	fd_set testfds; 
	while(1)
	{
		result = connect_to_server(address,port);
		if (result < 0)
		{
			/*reconnect all time*/
			sleep(1);
			continue;
		}

		FD_SET(client_socket, &readfds);//将服务器端socket加入到集合中
		while(1) 
		{
			int fd; 
			int nread; 
			testfds = readfds;
			//printf("server waiting\n"); 

			/*阻塞，并测试文件描述符变动 */
			result = select(FD_SETSIZE, &testfds, NULL,NULL,NULL); 
			if(result < 0) 
			{ 
				perror("run_in_client:server5"); 
				close(ser2net_socket);
	            return -1;
			}
			else if (result == 0)
			{
				printf("time out\n");
				continue;
			}
			
			//printf("result =%d\n",result); 
			/*扫描所有的文件描述符*/
			for(fd = 0; fd < FD_SETSIZE; fd++) 
			{
				/*找到相关文件描述符*/
				if(FD_ISSET(fd,&testfds)) 
				{ 
					
			     	/*判断是否为服务器套接字，是则表示为客户请求连接。*/
					if(fd == client_socket) 
					{ 
						ioctl(fd, FIONREAD, &nread);//取得数据量交给nread
						result = read(fd, client_in, nread); 
						if (result > 0)
						{
							result = write(ser2net_socket,client_in,nread);
							if (result <= 0)
								return -1;
						}
						else
						{
							printf("server read failed,close\n");
							
							FD_CLR(client_socket, &readfds); 
							close(client_socket);
							client_socket = -1;
							break;
						}
					} 
					else if (fd == ser2net_socket)
					{
						int offset = 0;
						#if 1
						ioctl(fd, FIONREAD, &nread);//取得数据量交给nread
						read(fd, ser2net_in, nread); 
						#else
						int times = 2;
						while(times--)
						{
							nread=read(fd, ser2net_in+offset, 1500-offset); 
							offset +=nread;
							usleep(500);
						}
						nread = offset;
						#endif
						
						//printf("serving client on fd %d\n", fd); 
						/*write to all client*/
						if (nread >0)
						{
							result = write_ser2net_to_server(ser2net_in,nread);
							if (result < 0)
							{
								printf("server write failed,close\n");
								FD_CLR(client_socket, &readfds); 
								close(client_socket);
								client_socket = -1;
								//sleep(1);
								break;
							}
						}
						else
						{
							printf("serals no data read,close\n");
							
							FD_CLR(ser2net_socket, &readfds); 
							close(ser2net_socket);
							result = connect_ser2net();
							if (result != 0)
							{
								return -1;
							}
							FD_SET(ser2net_socket, &readfds);
						}
						
					}
					
				} 
				
			} 
			if (client_socket == -1)
			{
				printf("reconect server\n");
				break;
			}
		}
	}
}

int run_in_server(fd_set readfds)
{
	int server_sockfd, client_sockfd; 
	fd_set testfds; 
	int server_len, client_len; 
	int i;
	time_t rawtime=0;
	struct timeval tv; 
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	struct sockaddr_in server_address; 
	struct sockaddr_in client_address; 
	int result; 
	
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);//建立服务器端socket 
	server_address.sin_family = AF_INET; 
	server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
	server_address.sin_port = htons(LISTEN_PORT); 
	server_len = sizeof(server_address); 
	
	for(i=0;i<MAX_CLIENT_NUM;i++)
	{
		clientlists[i] = -1;
		clientlasttime[i] = 0;
	}
	
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len); 
	listen(server_sockfd, 5); 
	
	
	FD_SET(server_sockfd, &readfds);//将服务器端socket加入到集合中
	
	while(1) 
	{
		char ch;                
		int fd; 
		int nread; 
		struct timeval tv; 
		tv.tv_sec = 30;
		tv.tv_usec = 0;
		testfds = readfds;
		//printf("server waiting\n"); 

		/*阻塞，并测试文件描述符变动 */
		result = select(FD_SETSIZE, &testfds, NULL,NULL,&tv); 
		if(result < 0) 
		{ 
			perror("server5"); 
			close(ser2net_socket);
			close(server_sockfd);
            return -1;
		}
		else if (result == 0)
		{
			printf("time out\n");
			for(i=0;i<MAX_CLIENT_NUM;i++)
		    {
		        if (clientlists[i]>0)
		        {
					close(clientlists[i]); 
		            clientlists[i]=-1;
					clientlasttime[i]=0;
		            break;
		        }
		    }
			FD_ZERO(&readfds); 
			FD_CLR(server_sockfd, &readfds); 
			FD_CLR(ser2net_socket, &readfds); 
            FD_SET(server_sockfd, &readfds); 
			FD_SET(ser2net_socket, &readfds);
			continue;
		}
		
		time(&rawtime);
		for(i=0;i<MAX_CLIENT_NUM;i++)
        {
            if (clientlists[i]>0 && (rawtime - clientlasttime[i])>30)
            {
            	printf("client %d timeout\n",i);
            	FD_CLR(clientlists[i], &readfds); 
                close(clientlists[i]); 
				clientlists[i] = -1;
				clientlasttime[i] = 0;
				
            }
        }
		//printf("result =%d\n",result); 
		/*扫描所有的文件描述符*/
		for(fd = 0; fd < FD_SETSIZE; fd++) 
		{
			/*找到相关文件描述符*/
			if(FD_ISSET(fd,&testfds)) 
			{ 
				
		     	/*判断是否为服务器套接字，是则表示为客户请求连接。*/
				if(fd == server_sockfd) 
				{ 
					client_len = sizeof(client_address); 
					client_sockfd = accept(server_sockfd,(struct sockaddr *)&client_address, &client_len); 
					if (client_sockfd <= 0)
					{
						printf("invalid accept\n");
						break;
					}
					for(i=0;i<MAX_CLIENT_NUM;i++)
		            {
		                if (clientlists[i]<0)
		                {
		                    clientlists[i]=client_sockfd;
							clientlasttime[i] = rawtime;
		                    break;
		                }
		            }
					if(i == MAX_CLIENT_NUM)
		            {
		                printf("too many clients\n");
		                close(client_sockfd);
		            }
					else
					{
						FD_SET(client_sockfd, &readfds);//将客户端socket加入到集合中
						printf("adding client on fd %d\n", client_sockfd); 
					}
				} 
				else if (fd == ser2net_socket)
				{
					ioctl(fd, FIONREAD, &nread);//取得数据量交给nread
					read(fd, ser2net_in, nread); 
					//printf("serving client on fd %d\n", fd); 
					/*write to all client*/
					if (nread >0)
						write_ser2net_to_allclient(ser2net_in,nread);
					else
					{
						printf("serals no data read,close\n");
						
						FD_CLR(ser2net_socket, &readfds); 
						close(ser2net_socket);
						result = connect_ser2net();
						if (result != 0)
						{
							close(server_sockfd);
							return -1;
						}
						FD_SET(ser2net_socket, &readfds);
					}
					
				}
				/*客户端socket中有数据请求时*/
				else 
				{
					for(i=0;i<MAX_CLIENT_NUM;i++)
		            {
		                if (fd == clientlists[i])
		                {
							clientlasttime[i] = rawtime;
		                    break;
		                }
		            }
					ioctl(fd, FIONREAD, &nread);//取得数据量交给nread
					
					/*客户数据请求完毕，关闭套接字，从集合中清除相应描述符 */
					if(nread == 0) 
					{ 
						
						close(fd); 
						FD_CLR(fd, &readfds); 
						for(i=0;i<MAX_CLIENT_NUM;i++)
			            {
			                if (clientlists[i] == fd)
			                {
			                    clientlists[i]=-1;
								clientlasttime[i] = 0;
								
			                    break;
			                }
			            }
						printf("removing client on fd %d\n", fd); 
					} 
					/*处理客户数据请求*/
					else 
					{
						result = read(fd, client_in, nread); 
						if (result > 0)
						{
							result = write(ser2net_socket,client_in,nread);
							if (result <= 0)
								return -1;
						}
					} 
				} 
			} 
		} 
	}

}


#define SER2MODE_SERVER      0
#define SER2MODE_CLIENT      1
int run_mode = SER2MODE_SERVER;
#if 0
int main(int argc ,char** argv) 
{ 
	int result; 
	fd_set readfds,testfds; 
	int options = 1;
	if (argc < 2)
	{
		printf("sertcpproxy mode [serverip] [port]\n");
		return -1;
	}
	result = connect_ser2net();
	if (result != 0)
	{
		return -1;
	}
	run_mode = atoi(argv[1]);
	
	FD_ZERO(&readfds);
	FD_SET(ser2net_socket, &readfds);//ser2net add to select;
	setsockopt(ser2net_socket, IPPROTO_TCP, TCP_NODELAY,(char *) &options, sizeof(options));
	
	if (run_mode == SER2MODE_SERVER)
		run_in_server(readfds);
	else if (run_mode == SER2MODE_CLIENT)
	{
		uint8_t *ipaddr;
		int port;
		ipaddr = argv[2];
		port = atoi(argv[3]);
		run_in_client(readfds,ipaddr,port);
	}
	else
	{
		printf("sertcpproxy mode=[0|1],0:server mode,1:client mode\n");
	}

	return 0;
} 
#else
extern 	int write_serial(char* buff,int len);
extern 	void  *serial_read_thread(void *param);
int run_in_client_fast(fd_set readfds,uint8_t *address,int port)
{
	int result = 0;
	fd_set testfds; 
	
	while(1)
	{
		result = connect_to_server(address,port);
		if (result < 0)
		{
			/*reconnect all time*/
			sleep(1);
			continue;
		}
		
		FD_SET(client_socket, &readfds);//将服务器端socket加入到集合中
		while(1) 
		{
			int fd; 
			int nread; 
			testfds = readfds;
			struct timeval tv; 
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			//printf("server waiting\n"); 

			/*阻塞，并测试文件描述符变动 */
			result = select(FD_SETSIZE, &testfds, NULL,NULL,&tv); 
			if(result < 0) 
			{ 
				perror("run_in_client:server5"); 
	            return -1;
			}
			else if (result == 0)
			{
				printf("time out\n");
				close(client_socket);
				client_socket = -1;
				break;
			}
			
			//printf("result =%d\n",result); 
			/*扫描所有的文件描述符*/
			for(fd = 0; fd < FD_SETSIZE; fd++) 
			{
				/*找到相关文件描述符*/
				if(FD_ISSET(fd,&testfds)) 
				{ 
					
			     	/*判断是否为服务器套接字，是则表示为客户请求连接。*/
					if(fd == client_socket) 
					{ 
						ioctl(fd, FIONREAD, &nread);//取得数据量交给nread
						if (nread == 0)
						{
							close(client_socket);
							client_socket = -1;
							break;
						}
						result = read(fd, client_in, nread); 
						if (result > 0)
						{
							write_serial(client_in,nread);
						}
						else
						{
							printf("server read failed,close\n");
							
							FD_CLR(client_socket, &readfds); 
							close(client_socket);
							client_socket = -1;
							break;
						}
					} 
				}
				
			} 
			if (client_socket == -1)
			{
				printf("reconect server\n");
				break;
			}
		}
	}
}

int main(int argc ,char** argv) 
{ 
	int result; 
	fd_set readfds,testfds; 
	int options = 1;
	int serial_thread = 0;
	if (argc < 2)
	{
		printf("sertcpproxy mode [serverip] [port]\n");
		return -1;
	}
	run_mode = atoi(argv[1]);
	
	FD_ZERO(&readfds);
	
	

	if (run_mode == SER2MODE_SERVER)
	{
		result = connect_ser2net();
		if (result != 0)
		{
			return -1;
		}
		FD_ZERO(&readfds);
		FD_SET(ser2net_socket, &readfds);//ser2net add to select;
		setsockopt(ser2net_socket, IPPROTO_TCP, TCP_NODELAY,(char *) &options, sizeof(options));
		run_in_server(readfds);
	}
	else if (run_mode == SER2MODE_CLIENT)
	{
		uint8_t *ipaddr;
		int port;
		// 创建串口线程 
	    result = pthread_create(&serial_thread, NULL, serial_read_thread, NULL); 
	    if ( 0 != result ) 
	    { 
	        printf("can't create thread:%s\n", strerror(result)); 
	        return -1;
	    }
		ipaddr = argv[2];
		port = atoi(argv[3]);
		run_in_client_fast(readfds,ipaddr,port);
	}
	else
	{
		printf("sertcpproxy mode=[0|1],0:server mode,1:client mode\n");
	}

	return 0;
} 

#endif


