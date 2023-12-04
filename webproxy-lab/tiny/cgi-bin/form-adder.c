/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  // 서버 프로세스가 만들어준 QEURY_STRING 환경변수를 getenv로 가져와서 buf에 넣는다.
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&');
    *p = '\0';
    sscanf(buf, "first=%d", &n1);
    sscanf(p+1, "second=%d", &n2);
  }

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
  printf("%s",content);
  fflush(stdout);

  exit(0);
}
/* $end adder */