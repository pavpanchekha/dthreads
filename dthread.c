#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>

#define WRAP(rtype, name, args, body) \
  rtype (* name##_orig) args;                                   \
  rtype name args {                                             \
    if (!name##_orig) name##_orig = dlsym(RTLD_NEXT, #name);    \
    body;                                                       \
  }

typedef struct dthread_t {
  void *(*start) (void *);
  void *arg;
  int status;
  void *retval;
} dthread_t;

dthread_t initial;
dthread_t *current = &initial;

typedef struct jmp_stack_t {
  jmp_buf *env;
  struct jmp_stack_t *next;
} jmp_stack_t;

jmp_stack_t *stack = NULL;

typedef struct dthread_cond_t {
  jmp_stack_t *waiting;
  int queued;
} dthread_cond_t;

WRAP(int, pthread_create, (pthread_t *thread, const pthread_attr_t *attr, void *(*start) (void *), void *arg), {
    dthread_t *st = malloc(sizeof(dthread_t));
    st->start = start;
    st->arg = arg;
    st->retval = 0;
    *thread = (pthread_t) st;
    return 0;
});

WRAP(void, pthread_exit, (void *status), {
    current->retval = status;
});

WRAP(int, pthread_equal, (pthread_t thread_1, pthread_t thread_2), {
    return thread_1 == thread_2;
});

WRAP(int, pthread_join, (pthread_t thread, void **status), {
    dthread_t *st = *(dthread_t**)(void*)&thread;
    //printf("Join dthread_t at %p\n", st);
    if (!st->retval) {
      dthread_t *master = current;
      current = st;
      jmp_buf *saved = malloc(sizeof(jmp_buf));
      if (!setjmp(*saved)) {
        jmp_stack_t *frame = malloc(sizeof(jmp_stack_t));
        frame->env = saved;
        frame->next = stack;
        stack = frame;
        st->retval = st->start(st->arg); 

      }
      jmp_stack_t *old_frame = stack;
      assert(old_frame->env == saved);
      stack = old_frame->next;
      free(old_frame);
      free(saved);
      current = master;
    }
    if (status) *status = st->retval;
    return 0;
});

WRAP(pthread_t, pthread_self, (void), {
    return (pthread_t)current;
});


WRAP(int, pthread_mutex_init, (pthread_mutex_t *mutex, const pthread_mutexattr_t *attr), {
    // There's no need to allocate mutexes, because only one thread is
    // running a time, and therefore mutex is automatically enforced.
    // We don't even track lock and unlock calls; this means some
    // formerly-deadlocking code will work, which isn't a tragedy.
    return 0;
});

WRAP(int, pthread_mutex_destroy, (pthread_mutex_t *mutex), {
    // Since we never allocated a mutex, we don't need to destroy it.
    return 0;
});

WRAP(int, pthread_mutex_lock, (pthread_mutex_t *mutex), {
    // Why bother locking a mutex, only to unlock it later?
    return 0;
});

WRAP(int, pthread_mutex_trylock, (pthread_mutex_t *mutex), {
    // Say you locked it. I mean, who cares?
    return 0;
});

WRAP(int, pthread_mutex_unlock, (pthread_mutex_t *mutex), {
    // Again, we're not keeping track.
    return 0;
});

/*
Could we use setjmp/longjmp to implement these?
 - When you cond_wait, setjmp, save in a queue, and longjmp up the stack
 - When you cond_signal, setjmp, save in the stack, and longjump down the queue
*/

WRAP(int, pthread_cond_init, (pthread_cond_t *cond, const pthread_condattr_t *attr), {
    dthread_cond_t *c = malloc(sizeof(dthread_cond_t));
    c->waiting = NULL;
    c->queued = 0;
    *cond = *(pthread_cond_t *)(void *)&c;
    return 0;
});

WRAP(int, pthread_cond_destroy, (pthread_cond_t *cond), {
    free(* (dthread_cond_t **) (void *) cond);
    return 0;
      
});

WRAP(int, pthread_cond_wait, (pthread_cond_t *cond, pthread_mutex_t *mutex), {
    dthread_cond_t *c = * (dthread_cond_t **) (void *) cond;
    if (c->queued == 0) {
      jmp_buf *saved = malloc(sizeof(jmp_buf));
      if (!setjmp(*saved)) {
        jmp_stack_t *frame = malloc(sizeof(jmp_stack_t));
        frame->env = saved;
        frame->next = c->waiting;
        c->waiting = frame;
        longjmp(*(stack->env), 1);
      } else {
        jmp_stack_t *old_frame = c->waiting;
        assert(old_frame->env == saved);
        c->waiting = old_frame->next;
        free(old_frame);
        free(saved);
      }
    } else {
      c->queued -= 1;
    }
    return 0;
});

WRAP(int, pthread_cond_timedwait, (pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime), {
    // For now, ignore the wait time
    return pthread_cond_wait(cond, mutex);
});

WRAP(int, pthread_cond_signal, (pthread_cond_t *cond), {
    dthread_cond_t *c = * (dthread_cond_t **) (void *) cond;
    if (c->waiting) {
      jmp_buf *saved = malloc(sizeof(jmp_buf));
      printf("Signaling %p\n", saved);
      if (!setjmp(*saved)) {
        jmp_stack_t *frame = malloc(sizeof(jmp_stack_t));
        frame->env = saved;
        frame->next = stack;
        stack = frame;
        longjmp(*(c->waiting->env), 1);
      } else {
        jmp_stack_t *old_frame = stack;
        assert(stack->env == saved);
        stack = old_frame->next;
        free(old_frame);
        free(saved);
      }
    } else {
      c->queued += 1;
    }
    return 0;
});

WRAP(int, pthread_cond_broadcast, (pthread_cond_t *cond), {
    dthread_cond_t *c = * (dthread_cond_t **) (void *) cond;
    while (c->waiting) pthread_cond_signal(cond);
});

/*
WRAP(int, pthread_once, (pthread_once_t *once_init, void (*init_routine)(void)), {
    return pthread_once_orig(once_init, init_routine);
});
*/
