/*
 * Copyright 2021, 2022. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위해 교육용으로 제작되었다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
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
int counter = 0;
/*
 * 생산된 아이템과 소비된 아이템의 개수를 기록하기 위한 변수
 */
int produced = 0;
int consumed = 0;
/*
 * 스핀락으로 사용한 원자변수 lock은 모든 생산자와 소비자가 공유한다.
 */
atomic_int lock = 0;
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
    int expected = 0;
    
    while (alive) {
        /*
         * 스핀락을 사용하여 먼저 락을 획득한 다음 버퍼에 빈 공간이 있는지 검사한다.
         * 버퍼에 빈 공간이 없으면 락을 풀고 다시 스핀락을 얻기 위해 루프를 반복한다.
         * 빈 공간이 있으면 루프를 빠져 나와 생산하기 위해 임계구역으로 들어간다.
         */
        while (true) {
            /*
             * alive 값을 검사하여 스레드가 비활성이면 즉시 종료시킨다.
             * 그 이유는 counter 값이 BUFSIZE인 상태에서 모든 소비자가 종료되면
             * 생산자는 이 while 루프를 빠저나오지 못해서 교착상태에 빠진다.
             */
            if (!alive)
                pthread_exit(NULL);
            /*
             * 임계구역에 진입하기 전에 락을 획득한다. 성공하면 루프를 빠져 나간다.
             */
            while(!atomic_compare_exchange_strong(&lock, &expected, 1))
                expected = 0;
            if (counter == BUFSIZE) {
                lock = 0;
                continue;
            }
            else
                break;
        }
        /*
         * 새로운 아이템(난수)을 생산하여 버퍼에 넣고 관련 변수를 갱신한다.
         */
        item = rand();
        buffer[in] = item;
        in = (in + 1) % BUFSIZE;
        counter++;
        produced++;
        /*
         * 임계구역에서 나왔으므로 락을 푼다.
         */
        lock = 0;
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
    int expected = 0;
    
    while (alive) {
        /*
         * 스핀락을 사용하여 먼저 락을 획득한 다음 버퍼에 아이템이 있는지 검사한다.
         * 버퍼에 아이템이 없으면 락을 풀고 다시 스핀락을 얻기 위해 루프를 반복한다.
         * 아이템이 있으면 루프를 빠져 나와 소비하기 위해 임계구역으로 들어간다.
         */
        while (true) {
            /*
             * alive 값을 검사하여 스레드가 비활성이면 즉시 종료시킨다.
             * 그 이유는 counter 값이 0인 상태에서 모든 생산자가 종료되면
             * 소비자는 이 while 루프를 빠저나오지 못해서 교착상태에 빠진다.
             */
            if (!alive)
                pthread_exit(NULL);
            /*
             * 임계구역에 진입하기 전에 락을 획득한다. 성공하면 루프를 빠져 나간다.
             */
            while(!atomic_compare_exchange_strong(&lock, &expected, 1))
                expected = 0;
            if (counter == 0) {
                lock = 0;
                continue;
            }
            else
                break;
        }
        /*
         * 버퍼에서 아이템(난수)을 꺼내서 출력하고 관련 변수를 갱신한다.
         */
        item = buffer[out];
        out = (out + 1) % BUFSIZE;
        counter--;
        consumed++;
        /*
         * 임계구역에서 나왔으므로 락을 푼다.
         */
        lock = 0;
        /*
         * 소비할 아이템을 빨간색으로 출력한다.
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
     * 자식 스레드가 종료될 때까지 기다린다.
     */
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);
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
