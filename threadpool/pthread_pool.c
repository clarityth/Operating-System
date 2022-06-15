/*
 * Copyright 2022. Heekuck Oh, all rights reserved
 * 이 프로그램은 한양대학교 ERICA 소프트웨어학부 재학생을 위한 교육용으로 제작되었습니다.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pthread_pool.h"
#include <unistd.h>

/*
 * 풀에 있는 일꾼(일벌) 스레드가 수행할 함수이다.
 * FIFO 대기열에서 기다리고 있는 작업을 하나씩 꺼내서 실행한다.
 */
static void *worker(void *param)
{
    // worker함수는 인자로 pool을 받는다. (pool에 포함된 락, task_t에 접근해야하기때문)
    pthread_pool_t* pool = (pthread_pool_t*) param;
    task_t fnc; // 실행할 함수 저장

    while(pool->running) {
        /* 뮤텍스락 획득 */
        pthread_mutex_lock(&pool->mutex);
        while(pool->running && pool->q_len == 0) {
            // 대기열이 비어있을 경우 full에서 새 작업이 들어올 때까지 기다림
            pthread_cond_wait(&pool->full, &pool->mutex);
        }
        if (!pool->running) {
            // 대기열 접근을 기다리다가 풀이 종료된 경우 -> 루프 종료
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        /* 실행할 함수와 인자를 fnc에 저장 */
        fnc.function = pool->q[pool->q_front].function;
        fnc.param = pool->q[pool->q_front].param;

        /* 대기열의 다음 실행 위치를 한칸 밀어주기 */
        pool->q_front = (pool->q_front + 1) % pool->q_size;
        --pool->q_len;

        // 대기열의 빈자리가 있음을 알려준다.
        pthread_mutex_unlock(&pool->mutex);
        pthread_cond_signal(&pool->empty); 

        // 작업 실행
        fnc.function(fnc.param);
    }
    pthread_exit(NULL);
}

/*
 * 스레드풀을 초기화한다. 성공하면 POOL_SUCCESS를, 실패하면 POOL_FAIL을 리턴한다.
 * bee_size는 일꾼(일벌) 스레드의 갯수이고, queue_size는 작업 대기열의 크기이다.
 * 대기열의 크기 queue_size가 최소한 일꾼의 수 bee_size보다 크거나 같게 만든다.
 */
int pthread_pool_init(pthread_pool_t *pool, size_t bee_size, size_t queue_size)
{
    int i;
    // 예외 처리
    if (bee_size > POOL_MAXBSIZE || queue_size > POOL_MAXQSIZE || queue_size <= 0 || bee_size <= 0){
        return POOL_FAIL;
    }
    // 대기열의 크기보다 큰 일꾼의 수 입력이 들어오면 대기열을 일꾼의 수와 같게 만듬 
    if (bee_size > queue_size)
        pool->q_size = bee_size;
    else
        pool->q_size = queue_size;

    // 동적 할당 및 변수 초기화
    pool->running = true;
    pool->q = (task_t *)malloc(sizeof(task_t)*(pool->q_size));
    pool->q_front = 0;
    pool->q_len = 0;
    pool->bee = (pthread_t *)malloc(sizeof(pthread_t)*(bee_size));
    pool->bee_size = bee_size;

    // 뮤텍스락과 조건변수 초기화
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->full, NULL);
    pthread_cond_init(&pool->empty, NULL);

    // 일꾼 스레드 생성
    for (i = 0; i < bee_size; i++) {
        // 스레드 생성 오류 발생시, 생성한 스레드풀을 shutdown하고, POOL_FAIL 리턴
        if (pthread_create(pool->bee+i, NULL, worker, (void*)pool) != 0) {
            pthread_pool_shutdown(pool);
            return POOL_FAIL;
        }
    }
    return POOL_SUCCESS;
}

/*
 * 스레드풀에서 실행시킬 함수와 인자의 주소를 넘겨주며 작업을 요청한다.
 * 스레드풀의 대기열이 꽉 찬 상황에서 flag이 POOL_NOWAIT이면 즉시 POOL_FULL을 리턴한다.
 * POOL_WAIT이면 대기열에 빈 자리가 나올 때까지 기다렸다가 넣고 나온다.
 * 작업 요청이 성공하면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_submit(pthread_pool_t *pool, void (*f)(void *p), void *p, int flag)
{
    int tail;   
    // 뮤텍스락 획득
    pthread_mutex_lock(&pool->mutex);
    // 대기열에 빈자리가 없을 경우
    if (pool->q_len == pool->q_size) {
        // flag가 POOL_NOWAIT이면서 대기열에 빈자리가 없으면 즉시 POOL_FULL 리턴
        if (flag == POOL_NOWAIT) {
            pthread_mutex_unlock(&pool->mutex);
            return POOL_FULL;
        }
        // flag가 POOL_WAIT이면
        else if (flag == POOL_WAIT) {
            // while문을 사용하여 빈자리가 생길때까지 기다리도록 조건변수 활용
            while(pool->running && pool->q_len == pool->q_size) {
                pthread_cond_wait(&(pool->empty), &(pool->mutex));
            }
            // 이후 상태 재확인
            if (!pool->running) {
                pthread_mutex_unlock(&pool->mutex);
                pthread_exit(NULL);
            }
        }
    }
    // 새 작업이 들어갈 위치 계산
    tail = (pool->q_front + pool->q_len) % pool->q_size;
    
    // 새 작업 대기큐에 넣는 작업
    pool->q[tail].param = p;
    pool->q[tail].function = f;

    // 대기열의 길이 하나 추가
    ++pool->q_len;
    
    // worker에 신호 보내주기
    pthread_mutex_unlock(&(pool->mutex));
    pthread_cond_signal(&pool->full);
    return POOL_SUCCESS;
}

/*
 * 모든 일꾼 스레드를 종료하고 스레드풀에 할당된 자원을 모두 제거(반납)한다.
 * 락을 소유한 스레드를 중간에 철회하면 교착상태가 발생할 수 있으므로 주의한다.
 * 부모 스레드는 종료된 일꾼 스레드와 조인한 후에 할당된 메모리를 반납한다.
 * 종료가 완료되면 POOL_SUCCESS를 리턴한다.
 */
int pthread_pool_shutdown(pthread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mutex);
    // 실행중인 스레드가 루프를 자연스럽게 빠져나오도록 함
    pool->running = false;
    
    // 모든 스레드들을 깨워서 join
    pthread_cond_broadcast(&pool->empty);
    pthread_cond_broadcast(&pool->full);
    pthread_mutex_unlock(&pool->mutex);
    
    for (int i = 0 ; i < pool->bee_size; i++) 
        pthread_join(pool->bee[i], NULL);

    // 메모리 반납 및 뮤텍스락, 조건변수 삭제
    if (pool->bee){
        free(pool->bee);
        free(pool->q);
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->empty);
        pthread_cond_destroy(&pool->full);
    }
    return POOL_SUCCESS;
}