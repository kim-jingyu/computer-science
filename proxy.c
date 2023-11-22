#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void *thread(void *vargp);
void doit(int clientfd);
void read_requsethdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
/* 테스트 환경에 따른 도메인,포트 번호 지정을 위한 상수이다. (0 할당시에 도메인,포트 번호가 고정되어서 외부에서 접속이 가능하다.) */
static const int is_local_test = 1;

int main(int argc, char **argv)
{
  int listenfd, *clientfd;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  signal(SIGPIPE, SIG_IGN);

  // rootp = (web_object_t *)calloc(1, sizeof(web_object_t));

  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 전달받은 포트 번호를 사용해서 수신 소켓을 생성한다.
  while (1)
  {
    clientlen = sizeof(clientaddr);
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 요청을 수신한다.
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    Pthread_create(&tid, NULL, thread, clientfd); // Concurrent Proxy
  }

  return 0;
}

void *thread(void *vargp)
{
  int clientfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(clientfd);
  Close(clientfd);
  return NULL;
}

void doit(int clientfd)
{
  int serverfd, content_length;
  char request_buf[MAXLINE], response_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr, filename[MAXLINE], cgiargs[MAXLINE];
  rio_t request_rio, response_rio;

  /* 클라이언트의 요청을 수신한다. */

  // Request Line 읽기 [Client -> Proxy]
  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n %s\n", request_buf);

  // 요청 라인 parsing을 통해서 'method, uri, hostname, port, path'를 찾는다.
  sscanf(request_buf, "%s %s", method, uri);
  parse_uri(uri, hostname, port, path);

  // Server에 전송하기 위해 요청 라인의 형식을 변경한다. 'method uri version' -> 'method path HTTP/1.0'
  sprintf(request_buf, "%s %s %s\r\n", method, path, "HTTP/1.0");

  // 지원하지 않는 method인 경우에는 예외 처리한다.
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body>\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>Tiny Web Server</em>\r\n", body);

  // print HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // error msg와 response body를 server socket을 통해 클라이언트에 보낸다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/*
  uri의 형태가 http://hostname:port/path 혹은 http://hostname/path 일때,
  uri를 hostname, port, path로 파싱하는 함수이다.
*/
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  // hostname의 시작 위치 포인터는 '//'가 있으면 // 뒤 (ptr+2)부터 시작이고, 없으면 uri부터 시작이다.
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  // port number의 시작 위치
  char *port_ptr = strchr(hostname_ptr, ':');
  // path 시작 위치
  char *path_ptr = strchr(hostname_ptr, '/');

  // port가 있는 경우
  if (port_ptr)
  {
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  }
  // port가 없는 경우
  else
  {
    if (is_local_test)
    {
      // port의 기본 값인 80으로 포트 번호를 설정한다.
      strcpy(port, "80");
    }
    else
    {
      strcpy(port, "8000");
    }

    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }
}