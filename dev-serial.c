
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<termios.h>
#include<errno.h>

#define FALSE -1
#define TRUE 0

int speed_arr[] = {B115200,B38400, B19200, B9600, B4800, B2400, B1200, B300,B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int name_arr[] = {115200,38400,  19200,  9600,  4800,  2400,  1200,  300, 38400, 19200,  9600, 4800, 2400, 1200,  300, };
void set_speed(int fd, int speed){
  int   i; 
  int   status; 
  struct termios   Opt;
  tcgetattr(fd, &Opt); 
  for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++) { 
    if  (speed == name_arr[i]) {     
      tcflush(fd, TCIOFLUSH);     
      cfsetispeed(&Opt, speed_arr[i]);  
      cfsetospeed(&Opt, speed_arr[i]);   
      status = tcsetattr(fd, TCSANOW, &Opt);  
      if  (status != 0) {        
        perror("tcsetattr fd1");  
        return;
      }    
      tcflush(fd,TCIOFLUSH);   
    }  
  }
}

int set_Parity(int fd,int databits,int stopbits,int parity)
{ 
	struct termios options; 
	if  ( tcgetattr( fd,&options)  !=  0) { 
		perror("SetupSerial 1");     
		return(FALSE);  
	}
	options.c_cflag &= ~CSIZE; 
	switch (databits) 
	{   
	case 7:		
		options.c_cflag |= CS7; 
		break;
	case 8:     
		options.c_cflag |= CS8;
		break;   
	default:    
		fprintf(stderr,"Unsupported data size\n"); return (FALSE);  
	}
	switch (parity) 
	{
		case 'n':
		case 'N':    
			options.c_cflag &= ~PARENB;   /* Clear parity enable */
			options.c_iflag &= ~INPCK;     /* Enable parity checking */ 
			break;  
		case 'o':   
		case 'O':     
			options.c_cflag |= (PARODD | PARENB); 
			options.c_iflag |= INPCK;             /* Disnable parity checking */ 
			break;  
		case 'e':  
		case 'E':   
			options.c_cflag |= PARENB;     /* Enable parity */    
			options.c_cflag &= ~PARODD;    
			options.c_iflag |= INPCK;       /* Disnable parity checking */
			break;
		case 'S': 
		case 's':  /*as no parity*/   
		    options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;break;  
		default:   
			fprintf(stderr,"Unsupported parity\n");    
			return (FALSE);  
	}  
	
	switch (stopbits)
	{   
		case 1:    
			options.c_cflag &= ~CSTOPB;  
			break;  
		case 2:    
			options.c_cflag |= CSTOPB;  
		   break;
		default:    
			 fprintf(stderr,"Unsupported stop bits\n");  
			 return (FALSE); 
	} 
	
	options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
    options.c_cflag &= ~(CSIZE|PARENB);
    options.c_cflag |= CS8;
    
	/* Set input parity option */ 
	if (parity != 'n')   
		options.c_iflag |= INPCK; 
	tcflush(fd,TCIFLUSH);
	options.c_cc[VTIME] = 1; 
	options.c_cc[VMIN] = 0; /* Update the options and do it NOW */
	if (tcsetattr(fd,TCSANOW,&options) != 0)   
	{
		perror("SetupSerial 3");   
		return (FALSE);  
	} 
	return (TRUE);  
}
char readbuff[1500];
int serial_fd = 0;
extern int client_socket;
void  *serial_read_thread(void *param)
{
	//printf("This program updates last time at %s   %s\n",__TIME__,__DATE__);
	//printf("STDIO COM1\n");
	//int serversocket = (int) param;
	int offset = 0;
	int nread = 0;
	serial_fd = open("/dev/ttyS0",O_RDWR);
	if(serial_fd == -1)
	{
		perror("serialport error\n");
		return NULL;
	}
	else
	{
		printf("open ");
		printf("%s",ttyname(serial_fd));
		printf(" succesfully\n");
	}

	set_speed(serial_fd,115200);
	if (set_Parity(serial_fd,8,1,'N') == FALSE)  {
		printf("Set Parity Error\n");
		return NULL;
	}
	//char buf[] = "fe55aa07bc010203040506073d";
	//write(fd,&buf,26);
		
	while(1)
	{
		if(offset > 1400)
		{
			//printf("time out\n");
			int sendbytes = offset;
			offset = 0;
			if(sendbytes > 0)
			{
				//printf("%s\n",readbuff);
				int ret = 0;
				if (client_socket != -1)
				{
					ret = write(client_socket,readbuff,sendbytes);
					if (ret < 0)
					{
						continue;
					}
				}
				continue;
			}
		}
		if((nread = read(serial_fd, readbuff+offset, 1))>0)
		{
			offset += nread;
			//printf("\nLen: %d\n",nread);
		}
		else if (nread == 0)
		{
			//printf("time out\n");
			int sendbytes = offset;
			offset = 0;
			if(sendbytes > 0)
			{
				//printf("%s\n",readbuff);
				int ret = 0;
				if (client_socket != -1)
				{
					ret = write(client_socket,readbuff,sendbytes);
					if (ret < 0)
					{
						continue;
					}
				}
				continue;
			}
		}
		else
		{
			printf("read serial error\n");
		}
	}
	close(serial_fd);
	return 0;
}


int write_serial(char* buff,int len)
{
	int ret = 0;
	ret = write(serial_fd,buff,len);
	if (ret <= 0)
		return -1;
	return ret;
}

