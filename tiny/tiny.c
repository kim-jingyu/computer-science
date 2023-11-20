/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <stdio.h>

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
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  // 컨텐츠의 유형이 정적인지, 동적인지를 파악한 후에 각각의 서버에 보낸다.
  if (is_static) { // 정적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 일반 파일이 아니거나, 읽을 권한이 없으면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // reponse header의 content-length를 위해 정적 서버에 파일의 사이즈를 같이 보낸다
    serve_static(fd, filename, sbuf.st_size);
  } else {  // 동적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 일반 파일이 아니거나, 실행 파일이 아니면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
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
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // error msg와 response body를 server socket을 통해 클라이언트에 보낸다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  // 버퍼 rp의 마지막 끝을 만날 때까지(Content-Length의 마지막 \r\n) 계속 출력해줘서 없앤다.
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// uri를 받아서 요청받은 파일의 이름과 요청 인자를 채워준다.
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  // uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 반환한다.
  // 예) GET /hi.jpg HTTP/1.1 --> uri에 cgi-bin이 없음

  /*
    static-content. 즉, uri안에 "cgi-bin"과 일치하는 문자열이 없음

    예시)
    uri : /hi.jpg
    ->
    cgiargs :
    filename : ./hi.jpg
  */
  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");    // 정적 컨텐츠이므로, cgiargs에는 아무것도 없다.
    strcpy(filename, ".");  // 현재 경로에서부터 시작한다. ./path ~~
    strcat(filename, uri);  // filename 스트링에 uri 스트링을 이어붙인다.

    // 만약 uri뒤에 '/'이 있으면, 그 뒤에 home.html을 붙인다.
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");
    }

    // 정적 컨텐츠이면 1을 반환한다.
    return 1;
  } 
  /*
    dynamic-content

    예시)
    uri : /cgi-bin/adder?123&123
    ->
    cgiargs : 123&123
    filename : ./cgi-bin/adder
  */
  else {
    ptr = index(uri, '?');

    // '?'이 있으면 cgiargs를 '?' 뒤의 인자들과 값으로 채워주고, '?'를 NULL으로 만든다.
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else {
      // '?'이 없으면 아무것도 안넣어준다.
      strcpy(cgiargs, "");
    }

    strcpy(filename, '.');  // 현재 디렉토리에서 시작한다.
    strcat(filename, uri);  // uri를 넣어준다.
  }

  return 0;
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 클라이언트에게 응답 헤더 보내기
  
  // 응답 라인과 헤더를 작성한다.
  get_filetype(filename, filetype); // 파일 타입 찾아오기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 응답 라인 작성하기
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);  // 응답 헤더 작성하기
  sprintf(buf, "%sConnection : close\r\n", buf);
  sprintf(buf, "%sContent-Length : %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-Type : %s\r\n\r\n", buf, filetype);

  // 응답 라인과 헤더를 클라이언트에게 보낸다.
  Rio_writen(fd, buf, strlen(buf)); // connfd를 통해서 clientfd에게 보낸다.
  printf("Response header:\n"); // 서버에서도 출력
  printf("%s", buf);

  // 클라이언트에게 응답 바디 보내기
  srcfd = Open(filename, O_RDONLY, 0);  // filename의 이름을 갖는 파일을 읽기 권한으로 불러온다.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리에 파일 내용을 동적 할당한다.
  srcp = (char*)Malloc(filesize);   // 파일 크기만큼 메모리를 동적 할당한다.
  Rio_readn(srcfd, srcp, filesize); // filename 내용을 동적 할당한 메모리에 쓴다.
  Close(srcfd); // 파일을 닫는다.
  Rio_writen(fd, srcp, filesize); // 해당 메모리에 있는 파일 내용들을 fd에 보낸다.(=읽는다)
  // Munmap(srcp, filesize);
  free(srcp);
}

// Response header의 Conten-Type에 필요한 함수로, filename을 조사해서 각각의 식별자에 맞는 MIME 타입을 filetype에 입력해준다.
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "iamge/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filename, "text/plain");
  }
}

// 클라이언트가 원하는 동적 컨텐츠 디렉토리를 받아온다. 응답 라인과 헤더를 작성하고 서버에게 보내면, CGI 자식 프로세스를 fork하고, 그 프로세스의 표준 출력을 클라이언트 출력과 연결한다.
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  // return first part of HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1);

    // 클라이언트의 표준 출력을 CGI 프로그램의 표준 출력과 연결한다. 따라서 앞으로 CGI 프로그램에서 printf하면 클라이언트에서 출력된다.
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}