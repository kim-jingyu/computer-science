/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

#define strcasecmp _stricmp

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

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 클라이언트에게서 받은 요청
  char filename[MAXLINE], cgiargs[MAXLINE]; // parse_uri를 통해서 채워진다.
  rio_t rio;

  // 클라이언트가 rio로 보낸 request 라인과 헤더를 읽고 분석한다.
  Rio_readinitb(&rio, fd);  // rio 버퍼와 서버의 connfd를 연결시켜준다.
  Rio_readlineb(&rio, buf, MAXLINE);  // rio에 있는 응답라인 한 줄을 모두 buf로 옮긴다.
  printf("Request headers:\n");
  printf("%s", buf);  // 요청 라인 buf = "GET /hi HTTP/1.1\0"을 표준 출력해준다.
  sscanf(buf, "%s %s %s", method, uri, version);  // buf에서 문자열 3개를 읽어와서 method, uri, version이라는 문자열에 저장한다.

  // 요청 method가 GET이 아니면 종료한다. 즉, main으로 가서 연결을 닫고, 다음 요청을 기다린다.
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 요청 라인을 뺀 나머지 요청 헤더들은 무시한다.
  read_requesthdrs(&rio);

  // 클라이언트 요청 라인에서 받아온 uri를 이용해서 정적/동적 컨텐츠를 구분한다. 정적 컨텐츠면 1이 저장된다.
  is_static = parse_uri(uri, filename, cgiargs);

  // stat 함수는 file의 상태를 buffer에 넘긴다. 여기서 filename은 클라이언트가 요청한 서버의 컨텐츠 디렉토리 및 파일 이름이다.
  // 여기서 못넘기면 파일이 없다는 뜻이므로, 404 fail이다.
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "서버에 요청하신 파일이 없습니다.");
    return;
  }

  // 컨텐츠의 유형이 정적인지, 동적인지를 파악한 후에 각각의 서버에 보낸다.
  if (is_static) { // 정적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 일반 파일이 아니거나, 읽을 권한이 없으면
      clienterror(fd, filename, "403", "Forbidden", "권한없는 접근입니다.");
      return;
    }
    // reponse header의 content-length를 위해 정적 서버에 파일의 사이즈를 같이 보낸다
    serve_static(fd, filename, sbuf.st_size);
  } else {  // 동적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 일반 파일이 아니거나, 실행 파일이 아니면
      clienterror(fd, filename, "403", "Forbidden", "권한없는 접근입니다.");
      return;
    }

    // 동적 서버에 인자를 같이 보낸다.
    serve_dynamic(fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
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
  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body));

  // error msg와 response body를 server socket을 통해 클라이언트에 보낸다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}