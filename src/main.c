#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include "pandora_client.h"

int callback(void* handle , int cmd , void* param , void* userp)
{
	if(!handle  || ! userp || !param)
	{
		printf(" Bad Parameter\n");
		return -1;
	}



	printf("%d \n", (int)userp);
	PandoraPic *pic = (PandoraPic*)param;

	printf("OK , There is a picture , %d\n" , pic->header.len);

	free(pic->yuv);
	free(pic);
}


int main(int argc , char**argv)
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;//设定接受到指定信号后的动作为忽略
	sa.sa_flags = 0;
	if (sigemptyset(&sa.sa_mask) == -1 ||  
		sigaction(SIGPIPE, &sa, 0) == -1) {  
		perror("failed to ignore SIGPIPE; sigaction");
		exit(-1);
	}

	void* handle = PandoraClientNew(argv[1] , atoi(argv[2]) , callback , (void*)1);
	if(!handle)
	{
		printf("Client Create Failed\n");
		return -1;
	}

	while(1)
	{
		sleep(1);
	}
}