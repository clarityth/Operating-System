/*
 * Copyright 2021, 2022. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위해 교육용으로 제작되었다.
 */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

#define N 8
/*
 * ANSI 컬러 코드: 출력을 쉽게 구분하기 위해서 사용한다.
 * 순서대로 BLK, RED, GRN, YEL, BLU, MAG, CYN, WHT, RESET을 의미한다.
 */
char *color[N+1] = {"\e[0;30m","\e[0;31m","\e[0;32m","\e[0;33m","\e[0;34m","\e[0;35m","\e[0;36m","\e[0;37m","\e[0m"};
/*
 * 스핀락으로 사용할 원자변수 lock은 모든 스레드가 공유한다.
 * waiting[i]는 스레드 i가 임계구역에 들어가기 위해 기다리고 있음을 나타낸다.
 * alive 값이 false가 될 때까지 스레드 내의 루프가 무한히 반복된다.
 */
atomic_bool lock = false;
bool waiting[N];
bool alive = true;

/*
 * N 개의 스레드가 임계구역에 배타적으로 들어가기 위해 스핀락을 사용하여 동기화한다.
 */
void *worker(void *arg)
{
    int i = *(int *)arg;
    int j;
    bool expected, blocked;
    
    while (alive) {
        waiting[i] = blocked = true;
        expected = false;
        /*
         * 대기상태이고 임계구역이 차단되어 있으면 계속 돈다.
         */
        while (waiting[i] && blocked)
            if (atomic_compare_exchange_strong(&lock, &expected, true))
                blocked = false;
            else
                expected = false;
        /*
         * 임계구역에 들어왔으므로 더이상 기다리는 상태가 아니다.
         */
        waiting[i] = false;
        /*
         * 임계구역: 알파벳 문자를 한 줄에 40개씩 10줄 출력한다.
         */
        for (int k = 0; k < 400; ++k) {
            printf("%s%c%s", color[i], 'A'+i, color[N]);
            if ((k+1) % 40 == 0)
                printf("\n");
        }
        /*
         * 임계구역이 성공적으로 종료되었다.
         * 락을 풀지 않고 임계구역을 기다리는 다음 스레드 j를 찾는다.
         */
        j = (i+1) % N;
        while (j != i && !waiting[j])
            j = (j+1) % N;
        /*
         * 다음 대상 스레드가 자신이면 더 이상 기다리는 스레드가 없으므로 락을 푼다.
         * 그렇지 않으면 락을 건 상태로 j가 임계구역에 들어갈 수 있도록 한다.
         */
        if (j == i)
            lock = false;
        else
            waiting[j] = false;
    }
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t tid[N];
    int i, id[N];

    /*
     * N 개의 자식 스레드를 생성한다.
     */
    for (i = 0; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, worker, id+i);
    }
    /*
     * 스레드가 출력하는 동안 100 밀리초 쉰다.
     * 이 시간으로 스레드의 출력량을 조절한다.
     */
    usleep(100000);
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
     * 메인함수를 종료한다.
     */
    return 0;
}
