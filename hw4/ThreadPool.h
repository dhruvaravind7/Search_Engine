/*
 * Copyright ©2026 Justin Hsia and Amber Hu. All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Winter Quarter 2026 for
 * purposes of the course. No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored. Instructors
 * interested in reusing these course materials should contact the
 * authors.
 */

#ifndef HW4_THREADPOOL_H_
#define HW4_THREADPOOL_H_

#include <cstdint>   // for uint32_t, etc.
extern "C" {
#include <pthread.h>  // for the pthread threading/mutex functions
}
#include <list>       // for std::list

extern "C" {
#include "libhw1/CSE333.h"
}

namespace hw4 {

// A ThreadPool is, well, a pool of threads. ;)  A ThreadPool is an
// abstraction that allows customers to dispatch tasks to a set of
// worker threads.  Tasks are queued, and as a worker thread becomes
// available, it pulls a task off the queue and invokes a function
// pointer in the task to process it.  When it is done processing the
// task, the thread returns to the pool to receive and process the next
// available task.
class ThreadPool {
 public:
  // Construct a new ThreadPool with a certain number of worker
  // threads.  Arguments:
  //
  //  - num_threads:  the number of threads in the pool.
  explicit ThreadPool(uint32_t num_threads);
  virtual ~ThreadPool();

  // This inner class defines what a Task is.  A worker thread will
  // pull a task off the task queue and invoke the thread_task_fn
  // function pointer inside of it, passing it the Task* itself as an
  // argument.  The thread_task_fn takes ownership of the Task and
  // must arrange to delete the task when it is done.  Customers will
  // probably want to subclass Task to add task-specific fields to it.
  class Task;
  typedef void (*thread_task_fn)(Task* arg);

  class Task {
   public:
    // "func" is the task function that a worker thread should invoke to
    // process the task.
    explicit Task(thread_task_fn func) : func_(func) { }

    // Ensure any derived classes have the correct destructor called.
    virtual ~Task() { }

    // The dispatch function.
    thread_task_fn func_;
  };

  // Customers use Dispatch() to enqueue a Task for dispatch to a
  // worker thread.  Returns true if the work has been accepted, false
  // if it has been rejected (eg, the pool is shutting down).
  bool Dispatch(Task* t);

  // Customers use BeginShutdown() to stop accepting new Tasks and to tell
  // the pool's threads to exit out of their "wait for new work" loop.
  // Note that the threads themselves are not pthread_join()ed or
  // deallocated until the ThreadPool's destructor is called; BeginShutdown()
  // merely allows the pool's threads to gracefully finish in-flight work.
  // Finishing in-flight work but not accepting new work is sometimes known
  // as "lame duck mode".
  void BeginShutdown();

  // Customers can query if the pool has initiated a shutdown.
  bool IsShuttingDown();

  // This method is public so that a threadpool's workers can access them,
  // but is not intended for general consumption.  It implements each waiting
  // thread's "is there more work yet?" loop.
  void WorkerLoop();

 private:
  // A lock that worker threads and the Dispatch() method use to guard
  // the Task queue and other member variables.
  pthread_mutex_t lock_;

  // A condition variable to signal the threads in the Task queue that there
  // is new work in the work_queue_.
  pthread_cond_t q_cond_;

  // The queue of Tasks waiting to be dispatched to a worker thread.  The
  // tasks in this queue are owned by themselves; they clean themselves up
  // after they've finished running.
  std::list<Task*> work_queue_;

  // This should be set to "true" when it is time for the worker
  // threads to terminate, i.e., when the ThreadPool is
  // destroyed.  A worker thread will check this variable before
  // picking up its next piece of work; if it is true, the worker
  // threads will terminate.
  bool terminate_threads_;

  // This variable stores how many threads are currently running.  As
  // worker threads are born, they increment it, and as worker threads
  // terminates, they decrement it.
  uint32_t num_threads_running_;

  // The pthreads pthread_t structures representing each thread.
  pthread_t* thread_array_;
};

}  // namespace hw4

#endif  // HW4_THREADPOOL_H_
