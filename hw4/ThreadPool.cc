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

#include <unistd.h>
#include <iostream>

#include "./ThreadPool.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

namespace hw4 {

// This is the thread start routine, i.e., the function that threads
// are born into.
static void* ThreadLoop(void* t_pool);

ThreadPool::ThreadPool(uint32_t num_threads) {
  // Initialize our member variables.
  num_threads_running_ = 0;
  terminate_threads_ = false;
  Verify333(pthread_mutex_init(&lock_, nullptr) == 0);
  Verify333(pthread_cond_init(&q_cond_, nullptr) == 0);

  // Allocate the array of pthread structures.
  thread_array_ = new pthread_t[num_threads];

  // Spawn the threads one by one, passing them a pointer to self
  // as the argument to the thread start routine.
  Verify333(pthread_mutex_lock(&lock_) == 0);
  for (uint32_t i = 0; i < num_threads; i++) {
    Verify333(pthread_create(&(thread_array_[i]), nullptr, &ThreadLoop, this)
              == 0);
  }

  // Wait for all of the threads to be born and initialized.
  while (num_threads_running_ != num_threads) {
    Verify333(pthread_mutex_unlock(&lock_) == 0);
    sleep(1);  // give another thread the chance to acquire the lock
    Verify333(pthread_mutex_lock(&lock_) == 0);
  }
  Verify333(pthread_mutex_unlock(&lock_) == 0);

  // Done!  The thread pool is ready, and all of the worker threads
  // are initialized and waiting on q_cond_ to be notified of available
  // work.
}

ThreadPool::~ThreadPool() {
  // Let's be extra-certain that worker threads wake up and see the
  // terminate flag.
  BeginShutdown();

  Verify333(pthread_mutex_lock(&lock_) == 0);
  uint32_t num_threads = num_threads_running_;

  // Join with the running threads 1-by-1 until they have all died.
  for (uint32_t i = 0; i < num_threads; i++) {
    Verify333(pthread_mutex_unlock(&lock_) == 0);
    Verify333(pthread_join(thread_array_[i], nullptr) == 0);
    Verify333(pthread_mutex_lock(&lock_) == 0);
  }

  // All of the worker threads are dead, so clean up the thread
  // structures.
  Verify333(num_threads_running_ == 0);
  delete[] thread_array_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);

  // Empty the task queue, sequentially doing the remaining work ourselves.
  while (!work_queue_.empty()) {
    Task* nextTask = work_queue_.front();
    work_queue_.pop_front();
    nextTask->func_(nextTask);
  }
}

// Enqueue a Task for dispatch.
bool ThreadPool::Dispatch(Task* t) {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);

  if (!terminate_threads_) {
    work_queue_.push_back(t);
    Verify333(pthread_cond_signal(&q_cond_) == 0);
  }
  retval = !terminate_threads_;

  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

// Tell idle threads that they need to begin shutting down, and prevent
// new tasks from being added to the pool.
void ThreadPool::BeginShutdown() {
  Verify333(pthread_mutex_lock(&lock_) == 0);
  terminate_threads_ = true;
  Verify333(pthread_cond_broadcast(&q_cond_) == 0);
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

bool ThreadPool::IsShuttingDown() {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);
  retval = terminate_threads_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

// This is the main loop that all worker threads are born into.  All it does
// is jump back into the threadpool's member function.
void* ThreadLoop(void* t_pool) {
  ThreadPool* pool = reinterpret_cast<ThreadPool*>(t_pool);
  pool->WorkerLoop();
  return nullptr;
}

// Each worker waits for a signal on the work queue's condition variable, then
// they grab work off the queue.  Threads return (i.e., terminate) when
// they notice that terminate_threads_ is true.
void ThreadPool::WorkerLoop() {
  // Let the constructor know this new thread is alive.
  Verify333(pthread_mutex_lock(&lock_) == 0);
  num_threads_running_++;

  // This is our main thread work loop.
  while (!terminate_threads_) {
    // Wait to be signaled that something has happened.  When we return from
    // this function, it is because we've been signaled that there's work
    // in the queue *and* we hold the lock.
    Verify333(pthread_cond_wait(&q_cond_, &lock_) == 0);

    // Keep trying to dequeue work until the work queue is empty.
    while (!terminate_threads_ && !work_queue_.empty()) {
      Task* next_task = work_queue_.front();
      work_queue_.pop_front();

      // We've picked up a task.  Before we can invoke its function, we
      // need to release the lock so that other workers can also grab any
      // remaining work.  Once we're done with the task's function, grab
      // the lock again so that it's safe to check for more undone tasks.
      Verify333(pthread_mutex_unlock(&lock_) == 0);
      next_task->func_(next_task);
      Verify333(pthread_mutex_lock(&lock_) == 0);
    }
  }

  // All done, exit.
  num_threads_running_--;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

}  // namespace hw4
