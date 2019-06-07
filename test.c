#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

/* This function is run in two threads */
void *test1(void *vvptr) {
  int *vptr = (int *)vvptr;
  while(++(*vptr) < 100);
  return NULL;
}

pthread_mutex_t lock;

void *test2(void *vvptr) {
  int *vptr = (int *) vvptr;
  for (int i = 0; i < 100; i++) {
    pthread_mutex_lock(&lock);
    ++(*vptr);
    pthread_mutex_unlock(&lock);
  }
  return NULL;
}

pthread_cond_t cv;

void *test3(void *vvptr) {
  int *vptr = (int *) vvptr;
  pthread_mutex_lock(&lock);
  pthread_cond_wait(&cv, &lock);
  ++(*vptr);
  pthread_mutex_unlock(&lock);
  return NULL;
}

int TEST = 0;

void check(bool val) {
  TEST++;
  printf("Test %d: %s\n", TEST, (val ? "PASS" : "FAIL"));
}

int main() {
  /*** Test 1 ***/

  pthread_t inc_x_thread, inc_y_thread;
  int x = 0, y = 0;

  /* create a two threads which execute test1(&x) */
  assert(!pthread_create(&inc_x_thread, NULL, test1, &x));
  assert(!pthread_create(&inc_y_thread, NULL, test1, &y));
  assert(!pthread_join(inc_x_thread, NULL));
  assert(!pthread_join(inc_y_thread, NULL));

  check(x == 100 && y == 100);

  /*** Test 2 ***/

  pthread_t inc_thread_1, inc_thread_2;
  int z = 0;
  assert(!pthread_mutex_init(&lock, NULL));
  assert(!pthread_create(&inc_thread_1, NULL, test2, &z));
  assert(!pthread_create(&inc_thread_2, NULL, test2, &z));
  assert(!pthread_join(inc_thread_1, NULL));
  assert(!pthread_join(inc_thread_2, NULL));

  check(z == 200);

  /*** Test 3 ***/

  pthread_t thread;
  int w = 0;
  char cmd[1024];
  assert(!pthread_cond_init(&cv, NULL));
  assert(!pthread_create(&thread, NULL, test3, &w));
  scanf("%s", cmd);
  assert(!pthread_cond_signal(&cv));
  assert(!pthread_join(thread, NULL));

  check(w > 0);

  return 0;
}
