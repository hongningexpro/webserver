#ifndef __WEBSRV_H_
#define __WEBSRV_H_
/*
*头文件
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
*宏定义
*/
//错误处理宏
#define sys_err(err_info,err_no) \
{\
	perror(err_info);\
	return err_no;\
}
#define MAXPATH 256 //最大路径长度
#define MAXBUF  8192//最大缓冲区
//文件类型协议头
#define TEXT_TYPE "Content-Type:text/html\r\n\r\n"
#define JPG_TYPE  "Content-Type:image/jpg\r\n\r\n"
#define PNG_TYPE  "Content-Type:image/png\r\n\r\n"

typedef struct sockaddr         SA;
typedef struct sockaddr_in      SAI;

//web服务器对象
struct __TWebSrv{
	int       listenfd;//监听套接字
	char      workPath[MAXPATH];//web服务器工作目录
	int       port;
	int       logfd;//日志文件句柄
	SAI		  websrvAddr;//web服务器地址结构体
};
struct __TWebSrv *g_websrvObj;

//客户端请求信息
typedef struct __TRequest{
	char      method[16];//请求方式
	char      path[MAXPATH];//请求的路径
	char      proto[16];//协议类型
}Request;
//服务器响应信息
typedef struct __TResponse{
	char	  proto[16];//响应方式
	int       response_code;//应答码
	char      content_type[64];//文件类型
}Response;

/*
*函数声明
*/
int websrv_init();//web服务器初始化函数 返回0代表成功
void websrv_release();//web服务器销毁函数
int websrv_create(const char *ip);//web服务器创建函数,返回0代表成功
int websrv_handle_client();//web服务器处理客户端请求

static int getclient_request(int fd,Request *req);
static void getsrv_response(Request *req,Response *res);
static int websrv_response(int fd,Response *res,const char *file_path);
static int __websrv_response_4(int fd,Response *res);
static int __websrv_response_2(int fd,Response *res,const char *file_path);

static char *read_conf(char *server_info);//读取配置文件
static void write_log(int fd,const char *peer_ip,const char *now_time,const char *description);//写日志函数
#endif
