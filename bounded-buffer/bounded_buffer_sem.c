/*
 * Copyright 2021, 2022. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위해 교육용으로 제작되었다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

#define N 8
#define BUFSIZE 10
#define RED "\e[0;31m"
#define RESET "\e[0m"
/*
 * 생산자와 소비자가 공유할 버퍼를 만들고 필요한 변수를 초기화한다.
 */
int buffer[BUFSIZE];
int in = 0;
int out = 0;
/*
 * 생산된 아이템과 소비된 아이템의 개수를 기록하기 위한 변수이다.
 */
int produced = 0;
int consumed = 0;
/*
 * 네 개의 세마포를 필요에 따라 생산자와 소비자가 공유한다.
 */
#ifdef __APPLE__
sem_t *empty, *full, *pro_mutex, *con_mutex;
char ename[16], fname[16], pmname[16], cmname[16];
#elif __linux__ | __unix__
sem_t empty, full, pro_mutex, con_mutex;
#else
error "Unknown compiler"
#endif
/*
 * alive 값이 false가 될 때까지 스레드 내의 루프가 무한히 반복된다.
 */
bool alive = true;

/*
 * 생산자 스레드로 실행할 함수이다. 아이템(난수)을 생성하여 버퍼에 넣는다.
 */
void *producer(void *arg)
{
    int i = *(int *)arg;
    int item;
    
    while (alive) {
        /*
         * 버퍼에 빈 공간을 기다린 다음 임계구역에 들어가기 위해 뮤텍스락을 기다린다.
         */
#ifdef __APPLE__
        sem_wait(empty);
        sem_wait(pro_mutex);
#elif __linux__ | __unix__
        sem_wait(&empty);
        sem_wait(&pro_mutex);
#else
error "Unknown compiler"
#endif
        /*
         * 새로운 아이템(난수)을 생산하여 버퍼에 넣고 관련 변수를 갱신한다.
         */
        item = rand();
        buffer[in] = item;
        in = (in + 1) % BUFSIZE;
        produced++;
        /*
         * 임계구역에서 나왔으므로 뮤텍스락을 풀고 세마포 full 값을 증가시킨다.
         */
#ifdef __APPLE__
        sem_post(pro_mutex);
        sem_post(full);
#elif __linux__ | __unix__
        sem_post(&pro_mutex);
        sem_post(&full);
#else
error "Unknown compiler"
#endif
        /*
         * 생산한 아이템을 출력한다.
         */
        printf("<P%d,%d>\n", i, item);
    }
    pthread_exit(NULL);
}

/*
 * 소비자 스레드로 실행할 함수이다. 버퍼에서 아이템(난수)을 읽고 출력한다.
 */
void *consumer(void *arg)
{
    int i = *(int *)arg;
    int item;
     
    while (alive) {
        /*
         * 새 아이템이 버퍼에 채워지기를 기다린 다음 임계구역에 들어가기 위해 뮤텍스락을 기다린다.
         */
#ifdef __APPLE__
        sem_wait(full);
        sem_wait(con_mutex);
#elif __linux__ | __unix__
        sem_wait(&full);
        sem_wait(&con_mutex);
#else
error "Unknown compiler"
#endif
        /*
         * 버퍼에서 아이템(난수)을 꺼내고 관련 변수를 갱신한다.
         */
        item = buffer[out];
        out = (out + 1) % BUFSIZE;
        consumed++;
        /*
         * 임계구역에서 나왔으므로 뮤텍스락을 풀고 세마포 empty 값을 증가시킨다.
         */
#ifdef __APPLE__
        sem_post(con_mutex);
        sem_post(empty);
#elif __linux__ | __unix__
        sem_post(&con_mutex);
        sem_post(&empty);
#else
error "Unknown compiler"
#endif
        /*
         *  소비할 아이템을 빨간색으로 출력한다.
         */
        printf(RED"<C%d,%d>"RESET"\n", i, item);
    }
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t tid[N];
    int i, id[N];

    /*
     * 네 개의 세마포를 초기화한다.
     * 처음에는 버퍼가 모두 비어있으므로 세마포 empty 값을 BUFSIZE로, full을 0으로 놓는다.
     * con_mutex, pro_mutex는 뮤텍스락으로 사용할 바이너리 세마포이므로 1로 초기화한다.
     */
#ifdef __APPLE__
    strcpy(ename, "ERICAXXXXXX"); mktemp(ename);
    strcpy(fname, "ERICAXXXXXX"); mktemp(fname);
    strcpy(pmname, "ERICAXXXXXX"); mktemp(pmname);
    strcpy(cmname, "ERICAXXXXXX"); mktemp(cmname);
    empty = sem_open(ename, O_CREAT, 0666, BUFSIZE);
    full = sem_open(fname, O_CREAT, 0666, 0);
    pro_mutex = sem_open(pmname, O_CREAT, 0666, 1);
    con_mutex = sem_open(cmname, O_CREAT, 0666, 1);
#elif __linux__ | __unix__
    sem_init(&empty, 0, BUFSIZE);
    sem_init(&full, 0, 0);
    sem_init(&pro_mutex, 0, 1);
    sem_init(&con_mutex, 0, 1);
#else
error "Unknown compiler"
#endif
    /*
     * N/2 개의 소비자 스레드를 생성한다.
     */
    for (i = 0; i < N/2; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, consumer, id+i);
    }
    /*
     * N/2 개의 생산자 스레드를 생성한다.
     */
    for (i = N/2; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, producer, id+i);
    }
    /*
     * 스레드가 출력하는 동안 1 밀리초 쉰다.
     * 이 시간으로 스레드의 출력량을 조절한다.
     */
    usleep(1000);
    /*
     * 스레드가 자연스럽게 무한 루프를 빠져나올 수 있게 한다.
     */
    alive = false;
    /*
     * while 루프의 조건이 false로 변경되어도 세마포를 기다리는 스레드가 있을 수 있다.
     * 다른 스레드가 종료되서 세마포 값을 증가시키지 못하면 그 스레드는 교착상태에 빠진다.
     * 이런 경우를 고려하여 모든 스레드를 강제로 철회한다.
     */
    for (i = 0; i < N; ++i)
        pthread_cancel(tid[i]);    
    /*
     * 자식 스레드가 종료될 때까지 기다린다.
     */
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);
    /*
     * 모든 세마포를 지운다.
     */
#ifdef __APPLE__
    sem_close(empty); sem_unlink(ename);
    sem_close(full); sem_unlink(fname);
    sem_close(pro_mutex); sem_unlink(pmname);
    sem_close(con_mutex); sem_unlink(cmname);
#elif __linux__ | __unix__
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&pro_mutex);
    sem_destroy(&con_mutex);
#else
error "Unknown compiler"
#endif
    /*
     * 생산된 아이템의 개수와 소비된 아이템의 개수를 출력한다.
     */
    printf("Total %d items were produced.\n", produced);
    printf("Total %d items were consumed.\n", consumed);
    /*
     * 메인함수를 종료한다.
     */
    return 0;
}
