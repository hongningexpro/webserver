#include "websrv.h"

static void setdeamon()
{
	pid_t pid;
	//成为一个新会话的首进程，失去控制终端
	if((pid = fork())<0)
	{
		perror("setdeamon fork");
		exit(-1);
	}
	if(pid != 0)//父进程退出
		exit(0);
	setsid();
	//改变当前目录
	/*if(chdir("/")<0)
	{
		perror("setdeamon chdir");
		exit(-1);
	}*/
	//设置umask为0
	umask(0);
	//重定向0，1，2文件描述符到/dev/null,因为失去了控制终端，再操作0，1没有意义
	close(0);
	open("/dev/null",O_RDWR);
	close(1);
	close(2);
	dup(0);
	dup(0);
}
int main(int argc,char *argv[])
{
	if(argc != 2)
	{
		fprintf(stderr,"%s [ip]\n",argv[0]);
		return -1;
	}
	setdeamon();
	websrv_init();
	if(websrv_create(argv[1])!=-1)
		websrv_handle_client();

	websrv_release();
	return 0;
}
