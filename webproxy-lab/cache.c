#include <stdio.h>
#include "csapp.h"
#include "cache.h"

web_object_t *rootp;
web_object_t *lastp;
int total_cache_size = 0;

// 캐싱된 웹 객체중에서 매칭할 path를 가진 객체를 반환
web_object_t *find_cache(char *path)
{
    // 캐시가 비었으면
    if (!rootp)
    {
        return NULL;
    }

    // 검사를 시작할 노드
    web_object_t *current = rootp;

    // 현재 검사중인 노드의 path가 찾는 path와 다르면 반복한다. 즉, 같아질 때까지 반복.
    while (strcmp(current->path, path))
    {
        // 현재 검사중인 노드의 다음 노드가 없으면 NULL을 반환한다.
        if (!current->next)
        {
            return NULL;
        }

        // 다음 노드로 이동한다.
        current = current->next;
        // path가 같은 노드를 찾으면 해당 객체를 반환한다.
        if (!strcmp(current->path, path))
        {
            return current;
        }
    }
    return current;
}

// web_object에 저장된 response를 client에 전송
void send_cache(web_object_t *web_object, int clientfd)
{
    // 1. Reponse Header 생성 및 전송
    char buf[MAXLINE];
    sprintf(buf, "HTTP/1.0 200 OK\r\n");                                           // status code
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);                            // server name
    sprintf(buf, "%sConnection: close\r\n", buf);                                  // way of connection
    sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, web_object->content_length); // content length
    Rio_writen(clientfd, buf, strlen(buf));

    // 2. 캐싱된 Response Body를 전송
    Rio_writen(clientfd, web_object->response_ptr, web_object->content_length);
}

// 사용한 web_object를 캐시 연결리스트의 root로 갱신
void read_cache(web_object_t *web_object)
{
    // 현재 노드가 이미 root이면 변경없이 종료한다.
    if (web_object == rootp)
        return;

    // 1. 현재 노드와 이전 그리고 다음 노드의 연결을 끊는다.

    // 이전 그리고 다음 노드가 모두 있는 경우
    if (web_object->next)
    {
        // 이전 노드와 다음 노드를 이어준다.
        web_object_t *prev_object = web_object->prev;
        web_object_t *next_object = web_object->next;

        if (prev_object)
        {
            web_object->prev->next = next_object;
        }
        web_object->next->prev = prev_object;
    }
    // 다음 노드가 없는 경우. 즉, 현재 노드가 마지막인 경우
    else
    {
        // 이전 노드와 현재 노드의 연결을 끊어준다.
        web_object->prev->next = NULL;
    }

    // 2. 현재 노드를 root로 변경한다.

    // root였던 노드는 현재 노드의 다음 노드가 된다.
    web_object->next = rootp;
    rootp = web_object;
}

// 인자로 전달된 웹 객체를 캐시 연결리스트에 추가
void write_cache(web_object_t *web_object)
{
    // total 캐시 사이즈에 현재 객체의 크기를 추가한다.
    total_cache_size += web_object->content_length;

    // 최대 총 캐시 크기를 초과한 경우에는 사용한지 가장 오래된 객체부터 제거한다.(LRU: Least Recently Used)
    while (total_cache_size > MAX_CACHE_SIZE)
    {
        total_cache_size -= lastp->content_length;
        // 마지막 노드를 마지막의 이전 노드로 변경한다.
        lastp = lastp->prev;
        // 제거한 노드의 메모리를 반환한다.
        free(lastp->next);
        lastp->next = NULL;
    }

    // 캐시 연결리스트가 빈 경우에는 lastp를 현재 객체로 지정한다.
    if (!rootp)
        lastp = web_object;

    // 캐시 연결리스트가 있는 경우에는 현재 객체를 root로 지정한다.
    if (rootp)
    {
        web_object->next = rootp;
        rootp->prev = web_object;
    }
    rootp = web_object;
}