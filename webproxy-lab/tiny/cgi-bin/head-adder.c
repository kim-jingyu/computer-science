/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p, *method;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // 서버 프로세스가 만들어준 QEURY_STRING 환경변수를 getenv로 가져와서 buf에 넣는다.
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  // 환경 변수로 넣어둔 요청 메서드를 확인한다.  
  method = getenv("REQUEST_METHOD");

  // content라는 string에 응답 본체를 담는다.
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  // CGI 프로세스가 부모인 서버 프로세스 대신에 채워주어야 하는 응답 헤더값
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");

  // 메서드가 HEAD가 아닐 경우만 응답 본체를 출력한다.
  if (!strcasecmp(method, "HEAD")) {
    printf("%s",content);
  }

  fflush(stdout);

  exit(0);
}
/* $end adder */
