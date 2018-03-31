#include "websrv.h"

int websrv_init()
{
	g_websrvObj = (struct __TWebSrv*)malloc(sizeof(struct __TWebSrv));
	if(g_websrvObj == NULL)
		sys_err("web server init failed",-1);
	memset(g_websrvObj,0,sizeof(struct __TWebSrv));
	return 0;
}

void websrv_release()
{
	if(g_websrvObj != NULL)
	{
		free(g_websrvObj);
		g_websrvObj = NULL;
	}
}

int websrv_create(const char *ip)
{
	int ret;
	char server_info[MAXBUF];
	char logpath[MAXPATH];
	struct tm *stime = NULL;
	time_t now_time;
	/*设置sigcld信号忽略，避免产生僵尸进程，父进程需要不断accept接受连接请求,以提高服务器效率*/
	signal(SIGCHLD,SIG_IGN);
	/*=======================================================================================*/
	/*读取配置文件*/
	if(!read_conf(server_info))
		sys_err("read_conf",-1);
	sscanf(server_info,"port:%d\nrelease path:%s\n",&g_websrvObj->port,g_websrvObj->workPath);
	/*=================================================*/
	/*创建日志*/
	time(&now_time);
	stime = gmtime(&now_time);
	sprintf(logpath,"%s/log/%d%d%d.log",g_websrvObj->workPath,1900+stime->tm_year,1+stime->tm_mon,stime->tm_mday);
	g_websrvObj->logfd = open(logpath,O_CREAT|O_RDWR|O_APPEND,0644);
	if(g_websrvObj->logfd == -1)
		sys_err("creat logfile failed",-1);
	/*=================================================*/
	g_websrvObj->listenfd = socket(AF_INET,SOCK_STREAM,0);
	if(g_websrvObj->listenfd == -1)
		sys_err("socket",-1);
	g_websrvObj->websrvAddr.sin_family = AF_INET;
	g_websrvObj->websrvAddr.sin_port = htons(g_websrvObj->port);
	g_websrvObj->websrvAddr.sin_addr.s_addr = inet_addr(ip);
	//设置地址复用
	int opt = 1;
	if(setsockopt(g_websrvObj->listenfd, SOL_SOCKET,SO_REUSEADDR, (const void *) &opt, sizeof(opt)))
		sys_err("setsockopt",-1);
	/*===================================================*/
	ret = bind(g_websrvObj->listenfd,(SA*)&g_websrvObj->websrvAddr,sizeof(SAI));
	if(ret == -1)
		sys_err("bind",-1);
	if(listen(g_websrvObj->listenfd,SOMAXCONN) == -1)
		sys_err("listen",-1);
	return 0;
}

int websrv_handle_client()
{
	int         ret;
	int         cfd;//客户端fd
	SAI         cliAddr;
	socklen_t   cli_len;
	Request     cli_request;	
	pid_t       pid;
	Response    srv_response;
	char        path[MAXPATH];
	time_t      now_time;
	strcpy(path,g_websrvObj->workPath);
	while(1)
	{
		bzero(&cli_request,sizeof(cli_request));
		bzero(&srv_response,sizeof(srv_response));
		cfd = accept(g_websrvObj->listenfd,(SA*)&cliAddr,&cli_len);
		if(cfd == -1)
			sys_err("accept",-1);
		pid = fork();	
		if(pid == -1)
			sys_err("fork",-1);
		if(pid == 0)
		{
			close(g_websrvObj->listenfd);
			//处理请求
			//1.获取客户端发来信息
			getclient_request(cfd,&cli_request);
			//2.分析客户端的请求
			getsrv_response(&cli_request,&srv_response);
			//3.根据客户端请求信息响应客户端请求
			ret = websrv_response(cfd,&srv_response,strcat(path,cli_request.path));
			/*4.写日志*/
			now_time = time(NULL);
			if(ret == -1)
				write_log(g_websrvObj->logfd,inet_ntoa(cliAddr.sin_addr),\
						  ctime(&now_time),"Server responsed failed\n");
			else if(ret == 0)
				write_log(g_websrvObj->logfd,inet_ntoa(cliAddr.sin_addr),\
						  ctime(&now_time),"Server responsed success\n");
			/*==================================*/
			exit(ret);
		}
		else if(pid > 0)
			close(cfd);
	}
}

static int getclient_request(int fd,Request *req)
{
	int ret = -1;
	char buf[MAXBUF];
again:
	bzero(buf,MAXBUF);
	while((ret = read(fd,buf,MAXBUF)) == -1)
	{
		if(errno == EINTR )
			goto again;
		else
			sys_err("read",ret);
	}
	sscanf(buf,"%s %s %s\r\n",req->method,req->path,req->proto);
	if(strcmp(req->path,"/") == 0)
		strcpy(req->path,"/index.html");
	return 0;	
}
static void getsrv_response(Request *req,Response *res)
{
	char path[MAXBUF];
	//1.拷贝协议版本
	strcmp(res->proto,req->proto);
	//2.确认要响应的信息码
	strcpy(path,g_websrvObj->workPath);
	strcat(path,req->path);
	res->response_code = access(path,R_OK)?404:200;
	if(res->response_code == 404)
	{
		sprintf(res->content_type,TEXT_TYPE);
		return;
	}
	//3.分析请求的文件类型
	if(strcmp(strrchr(req->path,'.'),".html") == 0)
		sprintf(res->content_type,TEXT_TYPE);
	else if(strcmp(strrchr(req->path,'.'),".jpg") == 0)
		sprintf(res->content_type,JPG_TYPE);
	else if(strcmp(strrchr(req->path,'.'),".png") == 0)
		sprintf(res->content_type,PNG_TYPE);
}

static int websrv_response(int fd,Response *res,const char *file_path)
{
	if(res->response_code == 404)
		return __websrv_response_4(fd,res);
	else if(res->response_code == 200)
		return __websrv_response_2(fd,res,file_path);
	return 0;
}
static int __websrv_response_4(int fd,Response *res)
{
	int ret = 0;
	char *head = "HTTP/1.1 404 Not Found\r\n";
	char *ctype = TEXT_TYPE;
	char *htmltxt = "<html><body>request not found</body></html>";
	if((ret = write(fd,head,strlen(head))) == -1)
		return ret;
	if((ret = write(fd,ctype,strlen(ctype))) == -1)
		return ret;
	if((ret = write(fd,htmltxt,strlen(htmltxt))) == -1)
		return ret;
	return 0;	
}
static int __websrv_response_2(int fd,Response *res,const char *file_path)
{
	int ret = 0;
	int ffd;
	char headbuf[MAXBUF];
	char htmlbuf[MAXBUF];
	if(strcmp(res->content_type,TEXT_TYPE) == 0)
	{
		sprintf(headbuf,"HTTP/1.1 200\r\n%s",TEXT_TYPE);
		write(fd,headbuf,strlen(headbuf));
		ffd = open(file_path,O_RDONLY);
		if(ffd == -1)
			sys_err("open",-1);
		while((ret = read(ffd,htmlbuf,MAXBUF))>0)
		{
			write(fd,htmlbuf,ret);
			bzero(htmlbuf,MAXBUF);
		}
		return 0;
	}
	else if(strcmp(res->content_type,JPG_TYPE) == 0)
	{
		sprintf(headbuf,"HTTP/1.1 200\r\n%s",JPG_TYPE);
		write(fd,headbuf,strlen(headbuf));
		FILE *fp = fopen(file_path,"rb");
		if(fp == NULL)
			sys_err("fopen",-1);
		while((ret = fread(htmlbuf,1,sizeof(htmlbuf),fp))>0)
		{
			write(fd,htmlbuf,ret);
			bzero(htmlbuf,sizeof(htmlbuf));
		}
		return 0;
	}
	else if(strcmp(res->content_type,PNG_TYPE) == 0)
	{
		sprintf(headbuf,"HTTP/1.1 200\r\n%s",PNG_TYPE);
		write(fd,headbuf,strlen(headbuf));
		FILE *fp = fopen(file_path,"rb");
		if(fp == NULL)
			sys_err("fopen",-1);
		while((ret = fread(htmlbuf,1,MAXBUF,fp))>0)
		{
			write(fd,htmlbuf,ret);
			bzero(htmlbuf,MAXBUF);
		}
		return 0;
	}
	return -1;
}
static char *read_conf(char *server_info)
{
	int len;
	int fd = open("server.conf",O_RDONLY);
	if(fd == -1)
		sys_err("open",0);
	len = read(fd,server_info,MAXBUF);
	if(len == -1)
		sys_err("read",0);
	return server_info;
}
static void write_log(int fd,const char *peer_ip,const char *now_time,const char *description)
{
	char logbuf[MAXBUF];
	sprintf(logbuf,"%s %s %s",peer_ip,now_time,description);
	write(fd,logbuf,strlen(logbuf));
}
