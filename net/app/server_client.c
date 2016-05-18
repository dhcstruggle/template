/*
*/
#include<stdio.h>  
#include<stdlib.h>  
#include<string.h>  
#include<errno.h>  
#include<sys/types.h>  
#include<sys/socket.h>  
#include<netinet/in.h>
#include<pthread.h>

#define DEFAULT_PORT 8000  
#define MAXLINE 4096  

pthread_t  clientThreadId;
void  *cmbs_thread_Client( void * pVoid )
{
	int  sockfd, n,rec_len;  
	char buf[MAXLINE];  
	int  ret;
	struct sockaddr_in *servaddr = (struct sockaddr_in *)pVoid;  

	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){  
		printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);  

	}

	if( connect(sockfd, (struct sockaddr*)servaddr, sizeof(struct sockaddr_in)) < 0){  
		printf("connect error: %s(errno: %d)\n",strerror(errno),errno);  
	}

	while(1)
	{
		printf("client:send msg to server: \n");
		if((rec_len = recv(sockfd, buf, MAXLINE,0)) == -1) {
			perror("recv error");
		}
		if( send(sockfd, buf, rec_len, 0) < 0)
		{
			printf("send msg error: %s(errno: %d)\n", strerror(errno), errno);
		}
		buf[rec_len]  = '\0';
		printf("client:Received : %s ",buf);
		memset(buf, 0, rec_len);
	}
}

int main(int argc, char** argv)  
{  
	int    socket_fd, connect_fd;  
	struct sockaddr_in     servaddr;  
	char    buff[4096];  
	int     n;  
	int ret;

	if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){  
		printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  

	memset(&servaddr, 0, sizeof(servaddr));  
	servaddr.sin_family = AF_INET;  
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(DEFAULT_PORT);


	if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){  
		printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  

	if( listen(socket_fd, 10) == -1){  
		printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  
	ret = pthread_create(&clientThreadId, NULL, &cmbs_thread_Client, &servaddr);

	printf("======waiting for client's request======\n");  
	
	if( (connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1){  
		printf("accept socket error: %s(errno: %d)",strerror(errno),errno);  
	}

	while(1)
	{  
		fgets(buff, 4096, stdin);

		if (buff[0] == 'q')
			break;

		if(send(connect_fd, buff, 4096,0) == -1)  
			perror("send error");  

		printf("server:send msg to client: %s\n", buff);

		n = recv(connect_fd, buff, MAXLINE, 0);  

		buff[n] = '\0';  
		printf("server:recv msg from client: %s\n", buff);  
	} 

	printf("======exit app======\n");  
	close(connect_fd);  
	close(socket_fd);  
}  

