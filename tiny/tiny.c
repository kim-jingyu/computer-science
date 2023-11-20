/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*
  port 번호를 인자로 받는다.
*/
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트에서 연결 요청 보내주면 알 수 있는 클라이언트의 연결 소켓 주소이다.

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다.
  listenfd = Open_listenfd(argv[1]);

  // 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어서 doit()을 호출한다.
  while (1) {
    // 클라이언트에서 바은 연결 요청을 accept한다.
    clientlen = sizeof(clientaddr);
    
    // connfd = 서버 연결 식별자
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept

    // 연결이 성공했다는 메시지를 위해서 Getnameinfo를 호출하면서 hostname과 port가 채워진다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // doit 함수를 실행한다.
    doit(connfd);   // line:netp:tiny:doit

    // 서버 연결 식별자를 닫아준다.
    Close(connfd);  // line:netp:tiny:close
  }
}
