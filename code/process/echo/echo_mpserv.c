#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUF_SIZE 30
void error_handling(char *message);
void read_childproc(int sig);

int main(int argc, char *argv[])
{
  int serv_sock, clnt_sock;
  struct sockaddr_in serv_adr, clnt_adr;

  pid_t pid;
  struct sigaction act;
  socklen_t adr_sz;
  int str_len, state;
  char buf[BUF_SIZE];
  
  // 检查启动参数
  if (argc != 2){
    printf("Usage : %s <port>\n", argv[0]);
    exit(1);
  }

  // 注册SIGCHLD信号
  act.sa_handler = read_childproc;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  state = sigaction(SIGCHLD, &act, 0);
  
  // 创建TCP门卫套接字serv_sock
  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  // 分配服务端地址
  memset(&serv_adr, 0, sizeof(serv_adr));
  serv_adr.sin_family = AF_INET;
  serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_adr.sin_port = htons(atoi(argv[1]));

  // 绑定套接字和地址
  if (bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
    error_handling("bind() error");
  // 进入监听状态，最大可受理请求数为5
  if (listen(serv_sock, 5) == -1)
    error_handling("listen() error");

  while (1)
  {
    // 从请求连接缓冲队列获取一个请求，并分配连接套接字clnt_sock
    adr_sz = sizeof(clnt_adr);
    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
    if (clnt_sock == -1)    // 若建立连接失败，则继续重新获取连接请求
      continue;
    else    // 建立连接成功
      puts("new client connected...");
    
    // 开启服务子进程
    pid = fork();
    if (pid == -1)    // 若创建子进程失败，关闭连接套接字，重新从请求队列获取新请求
    {
      close(clnt_sock);
      continue;
    }
    if (pid == 0)     // 子进程运行区域
    {
      // 关闭子进程fork的门卫套接字文件描述符
      close(serv_sock);
      
      // 读写连接套接字，完成echo服务
      while ((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0)
        write(clnt_sock, buf, str_len);

      // 关闭连接套接字
      close(clnt_sock);
      puts("client disconnected...");
      return 0;
    }
    else
    {
      // 父进程不提供实际服务，关闭连接套接字文件描述符
      close(clnt_sock);
    }
  }
  // 服务完毕，父进程关闭门卫套接字
  close(serv_sock);
  return 0;
}

// 子进程运行结束时的安全退出处理函数（防止僵尸进程）
void read_childproc(int sig)
{
  pid_t pid;
  int status;
  // waitpid安全退出
  pid = waitpid(-1, &status, WNOHANG);
  printf("removed proc id: %d \n", pid);
}

void error_handling(char *message)
{
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
