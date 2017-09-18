#include "pandora_client.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h> 
#include <setjmp.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

enum {
	WAIT_FOR_READ,
	WAIT_FOR_WRITE,
	WAIT_FOR_CONN
};

#define DEFAULT_TIMEOUT	10	/*secondes waitting for read/write*/

int sys_readn(int fd, void *vptr, int n)
{
    int nleft, nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;
            else
                return -1;
        }
        else if (nread == 0)
            break;

        nleft -= nread;
        ptr += nread;
    }

    return n - nleft;
}

int sys_writen(int fd, const void *vptr, int n)
{
    int nleft;
    int nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;   /* and call write() again */
            else
                return (-1);    /* error */
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

    return n;
}


int tcp_open(const char *ipaddr, int port)
{
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipaddr, &servaddr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, 
                sizeof(servaddr)) == -1) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/**
 *Description:check the socket  state
 *
 * @param 
 * fd: socket
 * timeout:the time out of select
 * wait_for:socket state(r,w,conn)
 *
 * @return 1 if everything was ok, 0 otherwise
 */
static int select_fd (int fd, int timeout, int wait_for)
{
	fd_set fdset;
	fd_set *rd = NULL, *wr = NULL;
	struct timeval tmo;
	int result;

	FD_ZERO (&fdset);
	FD_SET (fd, &fdset);
	if (wait_for == WAIT_FOR_READ)
	{
		rd = &fdset;
	}
	if (wait_for == WAIT_FOR_WRITE)
	{
		wr = &fdset;
	}
	if(wait_for == WAIT_FOR_CONN)
	{
		rd = &fdset;
		wr = &fdset;
	}

	tmo.tv_sec =  timeout;
	tmo.tv_usec = 0;
	do {
		result = select (fd + 1, rd, wr, NULL, &tmo);
	} while (result < 0 && errno == EINTR);
	
	return result;
}

typedef struct _PandoraClient_s{
	pthread_mutex_t		cliSocketLock;
	int					cliSocket;

	pthread_t 			receiveTask;
	pthread_t 			heartBeatTask;

	int 				exit;

	CallBack 			callback;
	void*				userp;
}PandoraClient;

struct thread_args {
	PandoraClient * client;
	char * ip;
	int port;
};


void PandoraClientTask(void* handle);
void PandoraClientHeartBeatTask(void* handle);

void parseHeader(char* header , int len , PandoraPicHeader* picHeader)
{
	int index = 0;
	picHeader->SOP[0] = header[index];
	picHeader->SOP[1] = header[index +1];
	index += 2;

	picHeader->pic_id = header[index];
	picHeader->type = header[index+1];
	index += 2;

	picHeader->width = (header[index + 0] & 0xff) << 24 | \
					(header[index + 1] & 0xff) <<  16  | \
					(header[index + 2] & 0xff) <<  8  | \
					(header[index + 3] & 0xff) <<  0  ;
	index += 4;

	picHeader->height = (header[index + 0] & 0xff) << 24 | \
					(header[index + 1] & 0xff) <<  16  | \
					(header[index + 2] & 0xff) <<  8  | \
					(header[index + 3] & 0xff) <<  0  ;
	index += 4;

	picHeader->timestamp = (header[index + 0] & 0xff) << 24 | \
					(header[index + 1] & 0xff) <<  16  | \
					(header[index + 2] & 0xff) <<  8  | \
					(header[index + 3] & 0xff) <<  0  ;
	index += 4;

	picHeader->len = (header[index + 0] & 0xff) << 24 | \
					(header[index + 1] & 0xff) <<  16  | \
					(header[index + 2] & 0xff) <<  8  | \
					(header[index + 3] & 0xff) <<  0  ;
}

void* PandoraClientNew(const char* ip , const int port , CallBack callback , void* userp)
{
	if(!ip || !callback || !userp )
	{
		printf("Bad Parameter\n");
		return NULL;
	}

	int ret = 0;

	PandoraClient *client = (PandoraClient*)malloc(sizeof(PandoraClient));
	if(!client)
	{
		printf("No Memory\n");
		return NULL;
	}

	memset(client , 0 , sizeof(PandoraClient));
	client->callback = callback;
	client->userp = userp;

	pthread_mutex_init(&client->cliSocketLock , NULL);

	// client->cliSocket = tcp_open(ip ,port);
	// if(client->cliSocket < 0)
	// {
	// 	printf("Connect to server failed\n");
	// 	free(client);
	// 	return NULL;
	// }

	while((client->cliSocket = tcp_open(ip, port)) < 0) {
		printf("Connect to server failed, retry after 5 seconds!\n");
		sleep(5);
	}

	printf("Connect to server successfully!\n");

	struct thread_args args;
	args.client = client;
	args.ip = ip;
	// memcpy(args.ip, ip, strlen(ip));
	args.port = port;

	ret = pthread_create(&client->receiveTask , NULL , (void*)PandoraClientTask , (void*)&args );
	if(ret != 0)
	{
		printf("Create Task Failed\n");
		free(client);
		return NULL;
	}

	ret = pthread_create(&client->heartBeatTask , NULL , (void*)PandoraClientHeartBeatTask , (void*)client );
	if(ret != 0)
	{
		printf("Create heart beat Task Failed\n");
		client->exit = 1;
		pthread_join(client->receiveTask , NULL);
		free(client);
		return NULL;
	}
	return (void*)client;
}

void PandoraCLientDestroy(void* handle)
{
	PandoraClient *client = (PandoraClient*)handle;
	if(!client)
	{
		printf("Bad Parameter\n");
		return;
	}

	client->exit = 0;
	pthread_join(client->heartBeatTask , NULL);
	pthread_join(client->receiveTask, NULL);
	free(client);
}

void PandoraClientHeartBeatTask(void* handle)
{
	PandoraClient *client = (PandoraClient*)handle;
	if(!client)
	{
		printf("Bad Parameter\n");
		return;
	}

	while(!client->exit)
	{
		int ret = write(client->cliSocket , "HEARTBEAT" , strlen("HEARTBEAT"));
		if(ret < 0)
		{
			printf("Write Heartbeat Error\n");
			// close(client->cliSocket);
			// break;
		}
		sleep(1);
	}
}

void PandoraClientTask(void* handle)
{
	struct thread_args *args = handle;

	PandoraClient *client = (PandoraClient*)args->client;
	if(!client)
	{
		printf("Bad Parameter\n");
		return;
	}
	// int connfd = client->cliSocket;

	while(!client->exit)
	{
		int ret = 0;
		ret = select_fd(client->cliSocket, 1, WAIT_FOR_READ);
		if(ret == 0)
		{
			printf("No Data\n");
			continue;
		}
		else if(ret > 0)
		{
			char header[64];
			int n = sys_readn(client->cliSocket , header , 2);
			if(header[0] != 0x47 || header[1] != 0x74)
			{
				printf("InValid Header SOP\n");
				printf("%02x %02x \n", header[0] , header[1]);
				exit(0);
				continue;
			}

			n = sys_readn(client->cliSocket , header + 2 , PANDORA_CLIENT_HEADER_SIZE - 2);

			PandoraPic* pic = (PandoraPic*)malloc(sizeof(PandoraPic));
			if(!pic)
			{
				printf("No Memory\n");
				continue;
			}

			parseHeader(header , n+2 , &pic->header);

			pic->yuv = malloc(pic->header.len);
			if(!pic->yuv)
			{
				printf("No Memory\n");
				free(pic);
				continue;
			}

			n = sys_readn(client->cliSocket , pic->yuv , pic->header.len);
			if(n != (int)pic->header.len)
			{
				printf("Read Error\n");
				free(pic->yuv);
				free(pic);
				exit(0);
			}

			if(client->callback)
				client->callback(client , 0 , pic , client->userp);
		}
		else
		{
			printf("Read Error, reconnecting...\n");
			// break;
			close(client->cliSocket);
			printf("Connect to server failed, retrying...!\n");
			while((client->cliSocket = tcp_open(args->ip, args->port)) < 0) {
				printf("Connect to server failed, retrying...!\n");
			}
		}
	}
}


