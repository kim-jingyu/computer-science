#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int status;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo("www.google.com", "80", &hints, &servinfo);
    printf(status);
}