#include <stdio.h>
#include "csapp.h"
#include "cache.h"

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

/* 4. Dealing with multiple concurrent requests */
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

  /* 1. 클라이언트의 요청을 수신한다. */

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

  /*
    caching 1. 요청 라인을 읽고나서, 캐싱된 요청(path)인지 확인한다.
  */
  web_object_t *cached_object = find_cache(path);
  // 만약 캐싱되어있다면
  if (cached_object)
  {
    send_cache(cached_object, clientfd); // 캐싱된 객체를 client에 전송한다.
    read_cache(cached_object);           // 사용한 웹 객체의 순서를 맨 앞으로 갱신한다.
    return;                              // Server로 요청을 보내지 말고 통신을 종료한다.
  }

  /* 2. 받은 요청을 End Server에 보낸다. */

  // Request Line 전송 (Proxy -> Server)
  // Server 소켓 생성
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_clientfd("43.201.84.24", port);
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", "Failed : proxy -> end server");
    return;
  }
  Rio_writen(serverfd, request_buf, strlen(request_buf));

  // Request Header 읽기 + 전송 (Client -> Proxy -> Server)
  read_requsethdrs(&request_rio, request_buf, serverfd, hostname, port);

  /* 3. End Server가 보낸 응답을 받고 Client에 전송한다. */

  // Reponse Header 읽기 + 전송 (Server -> Proxy -> Client). 응답 헤더를 한줄씩 읽으면서 바로 클라이언트에 전송한다.
  Rio_readinitb(&response_rio, serverfd);
  while (strcmp(response_buf, "\r\n"))
  {
    Rio_readlineb(&response_buf, response_buf, MAXLINE);
    // Reponse Body 수신에 사용하기 위해 Content-length를 저장한다.
    if (strstr(response_buf, "Content-length"))
    {
      content_length = atoi(strchr(response_buf, ":") + 1);
    }
    Rio_writen(clientfd, response_buf, strlen(response_buf));
  }

  // Response Body 읽기 + 전송 (Server -> Proxy -> Client). 응답 바디는 이진 데이터가 포함될 수 있으므로 한줄씩 읽지않고, Content-length만큼 한번에 읽고, 전송한다.
  response_ptr = malloc(content_length);
  Rio_readnb(&response_rio, response_ptr, content_length);
  Rio_writen(clientfd, response_ptr, content_length); // 클라이언트에 Response Body를 전송한다.
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

/*
  Request Header를 읽고, Server에 전송하는 함수이다.
  필수 헤더가 없는 경우에는 필수 헤더를 추가로 전송한다.
*/
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port)
{
  int is_host_exist;
  int is_connection_exist;
  int is_proxy_connection_exist;
  int is_user_agent_exist;

  // 첫번째 줄 읽기
  Rio_readlineb(request_rio, request_buf, MAXLINE);

  while (strcmp(request_buf, "\r\n"))
  {
    if (strstr(request_buf, "Proxy-Connection") != NULL)
    {
      sprintf(request_buf, "Proxy-Connection: close\r\n");
      is_proxy_connection_exist = 1;
    }
    else if (strstr(request_buf, "Connection") != NULL)
    {
      sprintf(request_buf, "Connection: close\r\n");
      is_connection_exist = 1;
    }
    else if (strstr(request_buf, "User-Agent") != NULL)
    {
      sprintf(request_buf, user_agent_hdr);
      is_user_agent_exist = 1;
    }
    else if (strstr(request_buf, "Host") != NULL)
    {
      is_host_exist = 1;
    }

    // Server에 전송
    Rio_writen(serverfd, request_buf, strlen(request_buf));
    // 다음 줄 읽기
    Rio_readlineb(request_rio, request_buf, MAXLINE);
  }

  // 필수 헤더 미포함 시, 추가로 전송한다.
  if (!is_proxy_connection_exist)
  {
    sprintf(request_buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_connection_exist)
  {
    sprintf(request_buf, "Connection: close\r\n");
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_host_exist)
  {
    if (!is_local_test)
    {
      hostname = "43.201.84.24";
    }
    sprintf(request_buf, "Host: %s:%s\r\n", hostname, port);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }
  if (!is_user_agent_exist)
  {
    sprintf(request_buf, user_agent_hdr);
    Rio_writen(serverfd, request_buf, strlen(request_buf));
  }

  // 종료문
  sprintf(request_buf, "\r\n");
  Rio_writen(serverfd, request_buf, strlen(request_buf));
  return;
}