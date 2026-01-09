/*
 * Copyright ©2025 Chris Thachuk & Naomi Alterman.  All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Autumn Quarter 2025 for
 * purposes of the course.  No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

#include "gtest/gtest.h"

extern "C" {
  #include "./LinkedList.h"
  #include "./LinkedList_priv.h"
}

#include "./test_suite.h"

namespace hw1 {

namespace {
// A comparator used to test sort.
int LLComparator(LLPayload_t p1, LLPayload_t p2) {
  if (p1 > p2)
    return 1;
  if (p1 < p2)
    return -1;
  return 0;
}
}  // anonymous namespace

class Test_LinkedList : public ::testing::Test {
 protected:
  // Code here will be called before each test executes (ie, before
  // each TEST_F).
  virtual void SetUp() {
    freeInvocations_ = 0;
  }

  // Code here will be called after each test executes (ie, after
  // each TEST_F)
  virtual void TearDown() {
    // Verify that none of the tests modifies any of the
    // testing constants.
    ASSERT_EQ((LLPayload_t)1U, kOne);
    ASSERT_EQ((LLPayload_t)2U, kTwo);
    ASSERT_EQ((LLPayload_t)3U, kThree);
    ASSERT_EQ((LLPayload_t)4U, kFour);
    ASSERT_EQ((LLPayload_t)5U, kFive);
  }

  // These values are used as payloads for the LinkedList tests.
  // They cannot be const, as stored value pointers are non-const.
  static LLPayload_t kOne, kTwo, kThree, kFour, kFive;

  // A stubbed and instrumented version of free() which counts how many
  // times it's been invoked; this allows us to make assertions without
  // actually freeing the payload (which had never been allocated in the
  // first place).  Note that the counter is reset in SetUp().
  static int freeInvocations_;
  static void StubbedFree(LLPayload_t payload) {
    // Do nothing but verify the payload is non-NULL and
    // increment the free count.
    ASSERT_TRUE(payload != NULL);
    freeInvocations_++;
  }
};  // class Test_LinkedList

// statics:
LLPayload_t Test_LinkedList::kOne = (LLPayload_t)1;
LLPayload_t Test_LinkedList::kTwo = (LLPayload_t)2;
LLPayload_t Test_LinkedList::kThree = (LLPayload_t)3;
LLPayload_t Test_LinkedList::kFour = (LLPayload_t)4;
LLPayload_t Test_LinkedList::kFive = (LLPayload_t)5;
int Test_LinkedList::freeInvocations_;

///////////////////////////////////////////////////////////////////////////////
// LinkedList tests
///////////////////////////////////////////////////////////////////////////////
TEST_F(Test_LinkedList, AllocateDeallocate) {
  HW1Environment::OpenTestCase();

  // Try creating a list.
  LinkedList* llp = LinkedList_Allocate();
  ASSERT_TRUE(llp != NULL);
  ASSERT_EQ(0, LinkedList_NumElements(llp));
  ASSERT_EQ(NULL, llp->head);
  ASSERT_EQ(NULL, llp->tail);
  HW1Environment::AddPoints(5);

  // Try deleting the (empty) list.
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  ASSERT_EQ(0, freeInvocations_);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, PushPop) {
  HW1Environment::OpenTestCase();

  // Create a list.
  LinkedList *llp = LinkedList_Allocate();
  ASSERT_TRUE(llp != NULL);
  ASSERT_EQ(0, LinkedList_NumElements(llp));
  ASSERT_EQ(NULL, llp->head);
  ASSERT_EQ(NULL, llp->tail);

  // Insert an element.
  LinkedList_Push(llp, kOne);
  ASSERT_EQ(1, LinkedList_NumElements(llp));
  ASSERT_EQ(llp->head, llp->tail);
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(NULL, llp->tail->next);
  ASSERT_EQ(kOne, llp->head->payload);
  HW1Environment::AddPoints(5);

  // Pop that element.
  LLPayload_t payload;
  ASSERT_TRUE(LinkedList_Pop(llp, &payload));
  ASSERT_EQ(kOne, payload);
  ASSERT_EQ(0, LinkedList_NumElements(llp));
  HW1Environment::AddPoints(5);

  // Try (and fail) to pop the element a second time.
  ASSERT_FALSE(LinkedList_Pop(llp, &payload));
  HW1Environment::AddPoints(5);

  // Try deleting the (empty) list.
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  ASSERT_EQ(0, freeInvocations_);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, Push_MultipleElements) {
  HW1Environment::OpenTestCase();

  // Insert three elements and ensure the pointers are all correct.
  LinkedList *llp = LinkedList_Allocate();
  LinkedList_Push(llp, kOne);
  ASSERT_EQ(1, LinkedList_NumElements(llp));
  ASSERT_EQ(llp->head, llp->tail);
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(NULL, llp->tail->next);
  ASSERT_EQ(kOne, llp->head->payload);
  HW1Environment::AddPoints(5);

  LinkedList_Push(llp, kTwo);
  ASSERT_EQ(2, LinkedList_NumElements(llp));
  ASSERT_NE(llp->head, llp->tail);
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(NULL, llp->tail->next);
  ASSERT_EQ(llp->tail, llp->head->next);
  ASSERT_EQ(llp->head, llp->tail->prev);
  ASSERT_EQ(kTwo, llp->head->payload);
  ASSERT_EQ(kOne, llp->tail->payload);
  HW1Environment::AddPoints(5);

  LinkedList_Push(llp, kThree);
  ASSERT_EQ(3, LinkedList_NumElements(llp));
  ASSERT_NE(llp->head, llp->tail);
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(NULL, llp->tail->next);
  ASSERT_EQ(llp->tail, llp->head->next->next);
  ASSERT_EQ(llp->head, llp->tail->prev->prev);
  ASSERT_EQ(kThree, llp->head->payload);
  ASSERT_EQ(kTwo, llp->head->next->payload);
  ASSERT_EQ(kOne, llp->head->next->next->payload);
  HW1Environment::AddPoints(5);

  // Free the non-empty list.
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  ASSERT_EQ(3, freeInvocations_);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, Pop) {
  HW1Environment::OpenTestCase();

  // Create a three-element list.
  LinkedList *llp = LinkedList_Allocate();
  LinkedList_Append(llp, kOne);
  LinkedList_Append(llp, kTwo);

  LLPayload_t payload;
  LinkedListNode *oldhead = llp->head;

  // Remove one element, then verify the list is still valid.
  ASSERT_TRUE(LinkedList_Pop(llp, &payload));
  ASSERT_EQ(kOne, payload);
  ASSERT_EQ(1, LinkedList_NumElements(llp));
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(NULL, llp->tail->next);
  ASSERT_NE(oldhead, llp->head);
  ASSERT_EQ(llp->tail, llp->head);
  HW1Environment::AddPoints(5);

  // Remove the last element in the list.
  ASSERT_TRUE(LinkedList_Pop(llp, &payload));
  ASSERT_EQ(kTwo, payload);
  ASSERT_EQ(0, LinkedList_NumElements(llp));
  ASSERT_EQ(NULL, llp->head);
  ASSERT_EQ(NULL, llp->tail);
  HW1Environment::AddPoints(5);

  // Try to remove another element from the empty list.
  ASSERT_FALSE(LinkedList_Pop(llp, &payload));
  ASSERT_EQ(0, LinkedList_NumElements(llp));
  HW1Environment::AddPoints(5);

  // Finally, free the empty list.
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  ASSERT_EQ(0, freeInvocations_);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, Slice_OneElement) {
  HW1Environment::OpenTestCase();

  // Create a single-element list.
  LinkedList *llp = LinkedList_Allocate();
  LinkedList_Append(llp, kOne);

  // Slice off the element and verify the list is now empty.
  LLPayload_t payload_ptr;
  ASSERT_TRUE(LLSlice(llp, &payload_ptr));
  ASSERT_EQ(kOne, payload_ptr);
  ASSERT_EQ(0, LinkedList_NumElements(llp));
  ASSERT_EQ(NULL, llp->head);
  ASSERT_EQ(NULL, llp->tail);
  HW1Environment::AddPoints(5);

  // (Fail to) slice an element out of the empty list.
  ASSERT_FALSE(LLSlice(llp, &payload_ptr));
  HW1Environment::AddPoints(5);

  // Free the empty list.
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  ASSERT_EQ(0, freeInvocations_);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, Sort) {
  HW1Environment::OpenTestCase();

  // Insert some elements.
  LinkedList *llp = LinkedList_Allocate();
  LinkedList_Append(llp, kThree);
  LinkedList_Append(llp, kTwo);
  LinkedList_Append(llp, kOne);
  ASSERT_EQ(3, LinkedList_NumElements(llp));

  // Sort ascending.
  LinkedList_Sort(llp, true, LLComparator);

  // Verify the sort.
  ASSERT_EQ(kOne, llp->head->payload);
  ASSERT_EQ(kTwo, llp->head->next->payload);
  ASSERT_EQ(kThree, llp->head->next->next->payload);
  ASSERT_EQ(NULL, llp->head->next->next->next);
  HW1Environment::AddPoints(5);

  // Resort descending.
  LinkedList_Sort(llp, false, &LLComparator);

  // Verify the sort.
  ASSERT_EQ(kThree, llp->head->payload);
  ASSERT_EQ(kTwo, llp->head->next->payload);
  ASSERT_EQ(kOne, llp->head->next->next->payload);
  ASSERT_EQ(NULL, llp->head->next->next->next);
  HW1Environment::AddPoints(5);

  // Clean up after the test.  No need to verify the deletion or award
  // points; we didn't modify the list, so we can rely on other tests.
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  ASSERT_EQ(3, freeInvocations_);
}

///////////////////////////////////////////////////////////////////////////////
// LLIterator tests
///////////////////////////////////////////////////////////////////////////////
TEST_F(Test_LinkedList, Iterator_EmptyList) {
  HW1Environment::OpenTestCase();

  LinkedList *llp = LinkedList_Allocate();
  LLIterator *lli = LLIterator_Allocate(llp);

  ASSERT_FALSE(LLIterator_IsValid(lli));

  LLIterator_Free(lli);
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, Iterator_Navigation) {
  HW1Environment::OpenTestCase();

  // Add some data to a list.
  LinkedList *llp = LinkedList_Allocate();
  LinkedList_Append(llp, kFive);
  LinkedList_Append(llp, kFour);
  LinkedList_Append(llp, kThree);
  LinkedList_Append(llp, kTwo);
  LinkedList_Append(llp, kOne);

  // Create the iterator for the list.
  LLIterator *lli = LLIterator_Allocate(llp);
  ASSERT_TRUE(lli != NULL);
  ASSERT_EQ(llp, lli->list);
  ASSERT_EQ(llp->head, lli->node);

  // Navigate using the iterator.
  LLPayload_t payload;
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kFive, payload);
  ASSERT_TRUE(LLIterator_Next(lli));
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kFour, payload);
  ASSERT_TRUE(LLIterator_Next(lli));
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kThree, payload);
  ASSERT_TRUE(LLIterator_Next(lli));
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kTwo, payload);
  ASSERT_TRUE(LLIterator_Next(lli));
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kOne, payload);
  HW1Environment::AddPoints(5);

  // Verify the iterator stops moving once we reach the end.
  ASSERT_FALSE(LLIterator_Next(lli));
  ASSERT_FALSE(LLIterator_IsValid(lli));
  HW1Environment::AddPoints(5);

  // Verify the iterator can be reset.
  LLIteratorRewind(lli);
  ASSERT_EQ(llp->head, lli->node);
  ASSERT_TRUE(LLIterator_IsValid(lli));
  HW1Environment::AddPoints(5);

  // Free the iterator and list.
  LLIterator_Free(lli);
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_LinkedList, Iterator_Deletion) {
  HW1Environment::OpenTestCase();

  // Add 4 elements to a list and create an iterator for it.
  LinkedList *llp = LinkedList_Allocate();
  LinkedList_Append(llp, kFour);
  LinkedList_Append(llp, kThree);
  LinkedList_Append(llp, kTwo);
  LinkedList_Append(llp, kOne);
  LLIterator *lli = LLIterator_Allocate(llp);
  LLPayload_t payload;

  // Delete from the front of the list and verify we still have a valid list.
  LinkedListNode *prev = NULL,
    *next = lli->node->next,
    *nextnext = lli->node->next->next;
  ASSERT_TRUE(LLIterator_Remove(lli, &Test_LinkedList::StubbedFree));
  ASSERT_EQ(next, llp->head);
  ASSERT_EQ(NULL, next->prev);
  ASSERT_EQ(nextnext, next->next);
  ASSERT_EQ(3, LinkedList_NumElements(llp));
  ASSERT_EQ(1, freeInvocations_);
  HW1Environment::AddPoints(5);

  // Verify the iterator is now pointing at what used to be the second element.
  ASSERT_EQ(next, lli->node);
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kThree, payload);
  HW1Environment::AddPoints(5);

  // Move the iterator forward by one element, to test removing from the
  // middle (ie, the 2nd element).
  prev = lli->node;
  ASSERT_TRUE(LLIterator_Next(lli));
  next = lli->node->next;
  nextnext = lli->node->next->next;

  ASSERT_TRUE(LLIterator_Remove(lli, &Test_LinkedList::StubbedFree));
  ASSERT_EQ(2, LinkedList_NumElements(lli->list));
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(NULL, llp->tail->next);
  ASSERT_EQ(llp->head->next, llp->tail);
  ASSERT_EQ(llp->tail->prev, llp->head);
  HW1Environment::AddPoints(5);

  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kOne, payload);
  ASSERT_EQ(llp->tail, lli->node);
  HW1Environment::AddPoints(5);

  // We're now at the tail position of a two-element list; remove that last
  // element and verify the list is still good.
  prev = lli->node->prev;
  ASSERT_TRUE(LLIterator_Remove(lli, &Test_LinkedList::StubbedFree));
  ASSERT_EQ(1, LinkedList_NumElements(lli->list));
  ASSERT_EQ(prev, llp->head);
  ASSERT_EQ(NULL, llp->head->prev);
  ASSERT_EQ(prev, llp->tail);
  ASSERT_EQ(NULL, llp->tail->next);
  HW1Environment::AddPoints(5);

  // And now, verify the iterator too.
  ASSERT_EQ(prev, lli->node);
  LLIterator_Get(lli, &payload);
  ASSERT_EQ(kThree, payload);
  HW1Environment::AddPoints(5);

  // Finally, remove the last node from the list.
  ASSERT_FALSE(LLIterator_Remove(lli, &Test_LinkedList::StubbedFree));
  ASSERT_EQ(0, LinkedList_NumElements(lli->list));
  ASSERT_EQ(NULL, lli->node);
  ASSERT_EQ(NULL, llp->head);
  ASSERT_EQ(NULL, llp->tail);
  HW1Environment::AddPoints(5);

  // Free the iterator and list.
  ASSERT_EQ(4, freeInvocations_);
  LLIterator_Free(lli);
  LinkedList_Free(llp, &Test_LinkedList::StubbedFree);
  HW1Environment::AddPoints(5);
}

}  // namespace hw1
