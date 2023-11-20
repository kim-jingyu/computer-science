#include "csapp.h"

int main(int argc, char **argv)
{
    struct addrinfo *p, *listp, hints;
    char buf[MAXLINE];
    int rc, flags;

    /* hint 구조체 세팅 */
    memset(&hints, 0, sizeof(struct addrinfo)); // hint 구조체 초기화
    hints.ai_family = AF_INET;                  /* IPv4 only */
    hints.ai_socktype = SOCK_STREAM;            /* Connections only */

    if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    flags = NI
        for (p = listp; p; p = p->ai_next)
    {
        Getnameinfo(p->ai_addr, p->ai_addrlen, // addrinfo 안의 IP주소(소켓 주소 구조체)를 찾아
                    buf, MAXLINE,              // 호스트 이름. flag를 썼으니 10진수 주소로.
                    NULL, 0,                   // service는 안받아오는듯
                    flags);
        printf("%s\n", buf); // input IP주소를 출력한다.
    }

    /* addrinfo 구조체를 free한다. */
    freeaddrinfo(listp);

    exit(0);
}