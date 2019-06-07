#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

/* This function is run in sixteen threads */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

const int N = 16;
const int MAX = 1 << 22;

void *increment() {
  for (int i = 0; i < MAX; i++) {
    pthread_mutex_lock(&lock);
    counter++;
    pthread_mutex_unlock(&lock);
  }
}

int main() {
  pthread_t threads[N];

  for (int i = 0; i < N; i++) {
    assert(!pthread_create(&threads[i], NULL, &increment, NULL));
  }
  for (int i = 0; i < N; i++) {
    assert(!pthread_join(threads[i], NULL));
  }

  assert(counter == N * MAX);

  return 0;
}
