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

#include <set>
#include <string>

#include "gtest/gtest.h"

extern "C" {
  #include "./HashTable.h"
  #include "./HashTable_priv.h"
  #include "./LinkedList.h"
  #include "./LinkedList_priv.h"
}
#include "./test_suite.h"

using std::set;
using std::string;

namespace hw1 {

///////////////////////////////////////////////////////////////////////////////
// Test fixture, payload struct, helper functions, etc
///////////////////////////////////////////////////////////////////////////////
static const int kMagicNum = 0xDEADBEEF;

typedef struct payload_st {
  int magic_num;
  int payload;
} TestPayload;

static TestPayload *NewPayload(int p) {
  TestPayload *np = static_cast<TestPayload *>(malloc(sizeof(TestPayload)));
  np->magic_num = kMagicNum;
  np->payload = p;
  return np;
}

// Insert key 'e' into 'table'.  This helper method assumes that 'e' does not
// currently exist in the table, and the value associated with key 'e' is
// also 'e'.
static void InsertElement(HashTable *table, int e) {
  HTKeyValue_t oldkv, newkv;
  newkv.key = e;
  newkv.value = NewPayload(e);

  ASSERT_FALSE(HashTable_Insert(table, newkv, &oldkv));
}

static HTKey_t AsKeyType(HTValue_t v) {
  return static_cast<HTKey_t>(static_cast<TestPayload *>(v)->payload);
}

static void FreeValue(HTValue_t v) {
  free(static_cast<TestPayload *>(v));
}

static void Reset(HTKeyValue_t *kv) {
  // We frequently need to initialize a HTKeyValue to a "known bad" value
  // to ensure that it's being overwritten later
  kv->key = static_cast<HTKey_t>(kMagicNum);
  kv->value = reinterpret_cast<HTValue_t>(kMagicNum);
}


class Test_HashTable : public ::testing::Test {
 protected:
  // Code here will be called before each test executes (ie, before
  // each TEST_F).
  virtual void SetUp() {
    freeInvocations_ = 0;
  }

  // A version of free() that verifies the payload before freeing it.
  static void VerifiedFree(HTValue_t payload) {
    ASSERT_EQ(kMagicNum, (static_cast<TestPayload *>(payload))->magic_num);
    free(payload);
  }

  // A version of VerifiedFree() which counts how many times it's been
  // invoked; this allows us to make assertions.  Note that the counter
  // is reset in SetUp().
  static int freeInvocations_;
  static void InstrumentedVerifiedFree(HTValue_t payload) {
    freeInvocations_++;
    VerifiedFree(payload);
  }
};  // class Test_HashTable

// statics:
int Test_HashTable::freeInvocations_;

// Insert, insert again, and find an element in the table.  This is a written
// as a helper function instead of a TEST_F() so that we can write tests
// that do this for multiple keys on the same table.  (hint, hint)
//
// 'k' is the key to insert, 'k_idx' is the index in the table's bucket list
// at which the table will insert 'k'.
static void TestInsertAndFind(HashTable *table, HTKey_t k, int k_idx) {
  HTValue_t v
    = reinterpret_cast<HTValue_t>(k * 100);  // we promptly overwrite this
                                             // element in the next section,
                                             // so don't bother
                                             // allocating/freeing memory for
                                             // a value
  HTKeyValue_t oldkv, newkv;
  Reset(&oldkv);
  newkv.key = k;
  newkv.value = v;

  LinkedList *pl = table->buckets[k_idx];
  int orig_list_size = LinkedList_NumElements(pl);

  // (0) Lookup the value we're about to insert.
  ASSERT_FALSE(HashTable_Find(table, k, &oldkv));

  // (1) Insert this key for the first time.
  ASSERT_FALSE(HashTable_Insert(table, newkv, &oldkv));
  ASSERT_EQ(orig_list_size + 1, LinkedList_NumElements(pl));
  ASSERT_EQ(static_cast<HTKey_t>(kMagicNum), oldkv.key);
  ASSERT_EQ(reinterpret_cast<HTValue_t>(kMagicNum), oldkv.value);

  // (students only have a Push() and an Append() method for modifying the
  // bucket's list, so we know that the new element is either the head or
  // the tail element)
  ASSERT_TRUE(static_cast<HTKeyValue_t*>(pl->head->payload)->key == k
              || static_cast<HTKeyValue_t*>(pl->tail->payload)->key == k);
  ASSERT_TRUE(static_cast<HTKeyValue_t*>(pl->head->payload)->value == v
              || static_cast<HTKeyValue_t*>(pl->tail->payload)->value == v);

  // Lookup the newly-inserted value.
  ASSERT_TRUE(HashTable_Find(table, k, &oldkv));
  ASSERT_EQ(k, oldkv.key);
  ASSERT_EQ(v, oldkv.value);


  // (2) Insert this key a second time, but use a dynamically-allocated value.
  // We compare the returned "old" element with the just-inserted
  // stack-allocated element, above.
  TestPayload *np = NewPayload(k);
  newkv.value = static_cast<HTValue_t>(np);

  ASSERT_TRUE(HashTable_Insert(table, newkv, &oldkv));
  ASSERT_EQ(orig_list_size + 1, LinkedList_NumElements(pl));
  ASSERT_EQ(k, oldkv.key);
  ASSERT_EQ(v, oldkv.value);

  // Lookup the ovewritten value.
  Reset(&oldkv);
  ASSERT_TRUE(HashTable_Find(table, k, &oldkv));
  ASSERT_EQ(k, oldkv.key);
  ASSERT_EQ(static_cast<HTValue_t>(np), oldkv.value);
}

// (Fail to) remove, insert, then successfully remove an element in the
// table.  As before, this is a written as a helper function instead of
// a TEST_F() so that we can write tests that do this for multiple keys on
// the same table.
//
// 'k' is the key to insert, 'k_idx' is the index in the table's bucket list
// at which the table will insert 'k', and 'k2' + 'k3' are other keys which
// also hash to k_idx.  Specifying these additional keys allows us to verify
// that we can remove elements which are in the middle of a bucket's chain.
static void TestRemove(HashTable *table, HTKey_t k, int k_idx,
                       HTKey_t k2, HTKey_t k3) {
  LinkedList *pl = table->buckets[k_idx];
  int orig_list_size = LinkedList_NumElements(pl);
  HTKeyValue_t oldkv;

  // (1) Remove a value that doesn't exist in the table.
  Reset(&oldkv);
  ASSERT_FALSE(HashTable_Remove(table, k + 1, &oldkv));
  ASSERT_EQ(static_cast<HTKey_t>(kMagicNum), oldkv.key);
  ASSERT_EQ(reinterpret_cast<HTValue_t>(kMagicNum), oldkv.value);
  ASSERT_EQ(orig_list_size, LinkedList_NumElements(pl));

  // (2) Insert this key and re-attempt the deletion.  Since students only
  // have a Push() and an Append() method for modifying a bucket's list, we
  // know that this MUST have removed either the head or the tail element
  InsertElement(table, k);
  ASSERT_EQ(orig_list_size + 1, LinkedList_NumElements(pl));
  ASSERT_TRUE(HashTable_Remove(table, k, &oldkv));
  ASSERT_EQ(orig_list_size, LinkedList_NumElements(pl));
  ASSERT_EQ(k, oldkv.key);
  ASSERT_EQ(k, AsKeyType(oldkv.value));
  FreeValue(oldkv.value);

  ASSERT_TRUE(pl->head == NULL ||
              (static_cast<HTKeyValue_t*>(pl->head->payload)->key != k
               && static_cast<HTKeyValue_t*>(pl->tail->payload)->key != k));
  if (pl->tail != NULL) {
    // These structs require so. much. casting.
    ASSERT_NE(k,
              AsKeyType(static_cast<HTKeyValue_t*>(pl->head->payload)->value));
    ASSERT_NE(k,
              AsKeyType(static_cast<HTKeyValue_t*>(pl->tail->payload)->value));
  }

  // (3) A second attempt to delete the already-deleted value should fail.
  Reset(&oldkv);
  ASSERT_FALSE(HashTable_Remove(table, k, &oldkv));
  ASSERT_EQ(orig_list_size, LinkedList_NumElements(pl));
  ASSERT_EQ(static_cast<HTKey_t>(kMagicNum), oldkv.key);
  ASSERT_EQ(reinterpret_cast<HTValue_t>(kMagicNum), oldkv.value);

  // (4) Insert k2, k, then k3.  This ensures k is in the middle of the chain
  // when we attempt its deletion.  Note that, unlike the previous section
  // where we know that neither the head nor the tail can be the deleted key,
  // this section must iterate through the entire chain to ensure the
  // deleted key is gone.
  InsertElement(table, k2);
  InsertElement(table, k);
  InsertElement(table, k3);
  ASSERT_EQ(orig_list_size + 3, LinkedList_NumElements(pl));

  Reset(&oldkv);
  ASSERT_TRUE(HashTable_Remove(table, k, &oldkv));
  ASSERT_EQ(orig_list_size + 2, LinkedList_NumElements(pl));
  ASSERT_EQ(k, oldkv.key);
  ASSERT_EQ(k, AsKeyType(oldkv.value));
  FreeValue(oldkv.value);
  for (LinkedListNode *n = pl->head; n != NULL; n = n->next) {
    ASSERT_NE(k, static_cast<HTKeyValue_t*>(n->payload)->key);
    ASSERT_NE(k, AsKeyType(static_cast<HTKeyValue_t*>(n->payload)->value));
  }

  // Clean up after our test.
  ASSERT_TRUE(HashTable_Remove(table, k2, &oldkv));
  FreeValue(oldkv.value);
  ASSERT_TRUE(HashTable_Remove(table, k3, &oldkv));
  FreeValue(oldkv.value);
}

///////////////////////////////////////////////////////////////////////////////
// HashTable tests
///////////////////////////////////////////////////////////////////////////////
TEST_F(Test_HashTable, AllocFree) {
  HW1Environment::OpenTestCase();

  HashTable *ht = HashTable_Allocate(3);
  ASSERT_EQ(0, ht->num_elements);
  ASSERT_EQ(3, ht->num_buckets);

  ASSERT_TRUE(ht->buckets != NULL);
  ASSERT_EQ(0, LinkedList_NumElements(ht->buckets[0]));
  ASSERT_EQ(0, LinkedList_NumElements(ht->buckets[1]));
  ASSERT_EQ(0, LinkedList_NumElements(ht->buckets[2]));
  HashTable_Free(ht, &Test_HashTable::VerifiedFree);

  HW1Environment::AddPoints(10);
}

TEST_F(Test_HashTable, InsertFind_Single) {
  HW1Environment::OpenTestCase();
  HashTable *table = HashTable_Allocate(8);

  TestInsertAndFind(table, 3, 3);

  HashTable_Free(table, &Test_HashTable::InstrumentedVerifiedFree);
  ASSERT_EQ(1, freeInvocations_);
  HW1Environment::AddPoints(10);
}

TEST_F(Test_HashTable, InsertFind_Multiple_AllBuckets) {
  static const int kTableSize = 10;
  static const int kNumIterations = kTableSize * 2.5;  // < resize_threshhold

  HW1Environment::OpenTestCase();
  HashTable *table = HashTable_Allocate(kTableSize);

  for (int i = 0; i < kNumIterations; i++) {
    SCOPED_TRACE(i);
    TestInsertAndFind(table, i, i % kTableSize);
  }

  HashTable_Free(table, &Test_HashTable::InstrumentedVerifiedFree);
  ASSERT_EQ(kNumIterations, freeInvocations_);
  HW1Environment::AddPoints(10);
}

TEST_F(Test_HashTable, Remove_Single) {
  HW1Environment::OpenTestCase();
  HashTable *table = HashTable_Allocate(12);

  TestRemove(table, 17 /* key */, 5 /* idx */,
             29, 41);  // other keys that hash to the same index

  HashTable_Free(table, &Test_HashTable::VerifiedFree);
  HW1Environment::AddPoints(10);
}

TEST_F(Test_HashTable, Remove_MultipleKVs_OneBucket) {
  static const int kTableSize = 17;
  static const int kNumIterations = kTableSize * 2.75;
  static const int kBucketIdx = 10;  // put all the keys in the 10th bucket

  HW1Environment::OpenTestCase();
  HashTable *table = HashTable_Allocate(kTableSize);

  // See previous test for explanation for why we start at i==1
  for (int i = 1; i < kNumIterations + 1; i++) {
    SCOPED_TRACE(i);
    TestRemove(table, i*kTableSize + kBucketIdx, kBucketIdx,  // key and idx
               (i+1) * kTableSize + kBucketIdx,  // two other keys
               (i+2) * kTableSize + kBucketIdx);
  }

  HashTable_Free(table, &Test_HashTable::VerifiedFree);
  HW1Environment::AddPoints(5);
}

///////////////////////////////////////////////////////////////////////////////
// HTIterator tests
///////////////////////////////////////////////////////////////////////////////
TEST_F(Test_HashTable, Iterator_EmptyTable) {
  HW1Environment::OpenTestCase();

  HashTable *table = HashTable_Allocate(300);
  HTIterator *it = HTIterator_Allocate(table);
  ASSERT_FALSE(HTIterator_IsValid(it));

  HTKeyValue_t oldkv;
  ASSERT_FALSE(HTIterator_Get(it, &oldkv));

  HashTable_Free(table, &Test_HashTable::VerifiedFree);
  HTIterator_Free(it);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_HashTable, Iterator_Navigate_ContiguousBuckets) {
  static const int kTableSize = 4;
  static const int kNumIterations = kTableSize * 2.5;

  HW1Environment::OpenTestCase();

  // Fill the table.
  HashTable *table = HashTable_Allocate(kTableSize);
  for (int i = 0; i < kNumIterations; i++) {
    InsertElement(table, i);
  }

  // Use the iterator to navigate through the values we added, verifying
  // that we see each value exactly once.
  HTIterator *it = HTIterator_Allocate(table);
  ASSERT_TRUE(HTIterator_IsValid(it));

  HTKeyValue_t oldkv;
  int num_times_seen[kNumIterations] = { 0 };   // array of 0's
  for (int i = 0; i < kNumIterations; i++) {
    ASSERT_TRUE(HTIterator_Get(it, &oldkv));

    // Verify we've never seen this key before, then increment the number
    // of times we've seen it.
    int key = static_cast<int>(oldkv.key);
    ASSERT_EQ(0, num_times_seen[key]);
    num_times_seen[key]++;

    // Now, verify the value.
    TestPayload *val = static_cast<TestPayload *>(oldkv.value);
    ASSERT_EQ(kMagicNum, val->magic_num);
    ASSERT_EQ(key, val->payload);

    // Finally, increment the iterator.
    if (i == kNumIterations - 1) {
      ASSERT_TRUE(HTIterator_IsValid(it));
      ASSERT_FALSE(HTIterator_Next(it));
      ASSERT_FALSE(HTIterator_IsValid(it));
    } else {
      ASSERT_TRUE(HTIterator_Next(it));
      ASSERT_TRUE(HTIterator_IsValid(it));
    }
  }

  // Verify each key was seen exactly once.
  for (int i = 0; i < kNumIterations; i++) {
    ASSERT_EQ(1, num_times_seen[i]);
  }

  HTIterator_Free(it);
  HashTable_Free(table, &Test_HashTable::InstrumentedVerifiedFree);
  ASSERT_EQ(kNumIterations, freeInvocations_);
  HW1Environment::AddPoints(10);
}

TEST_F(Test_HashTable, Iterator_Navigate_BucketListHasHoles) {
  // This table has 5 buckets, but the 0th, 2nd, and 3rd are empty (we only
  // inserted values into the 1st and 4th buckets).  This ensures that our
  // iterator can skip over "holes" in the bucket list.
  static const int kTableSize = 5;
  static const int kChainLength = 3;
  static const int kNumKeys = kChainLength * 2;

  HW1Environment::OpenTestCase();

  // Fill the table's 1st and 4th buckets, and create a dictionary to
  // remember the values that we added.
  HashTable *table = HashTable_Allocate(kTableSize);

  for (int i = 0; i < kChainLength; i++) {
    InsertElement(table, i * kTableSize + 1);
    InsertElement(table, i * kTableSize + 4);
  }

  // Use the iterator to navigate through the values we added, verifying
  // that we see each value exactly once.
  HTIterator *it = HTIterator_Allocate(table);
  ASSERT_TRUE(HTIterator_IsValid(it));

  HTKeyValue_t oldkv;
  set<int> seen_vals;
  for (int i = 0; i < kNumKeys; i++) {
    ASSERT_TRUE(HTIterator_Get(it, &oldkv));

    // Verify we've never seen this key before, then record it.
    int key = static_cast<int>(oldkv.key);
    ASSERT_EQ(0LU, seen_vals.count(key));
    seen_vals.insert(key);

    // Now, verify the value.
    TestPayload *val = static_cast<TestPayload *>(oldkv.value);
    ASSERT_EQ(kMagicNum, val->magic_num);
    ASSERT_EQ(key, val->payload);

    // Finally, increment the iterator.
    if (i == kChainLength * 2 - 1) {
      ASSERT_TRUE(HTIterator_IsValid(it));
      ASSERT_FALSE(HTIterator_Next(it));
      ASSERT_FALSE(HTIterator_IsValid(it));
    } else {
      ASSERT_TRUE(HTIterator_Next(it));
      ASSERT_TRUE(HTIterator_IsValid(it));
    }
  }

  // Lastly, verify that we didn't see any values that weren't in the table.
  ASSERT_EQ(HashTable_NumElements(table), static_cast<int>(seen_vals.size()));

  HTIterator_Free(it);
  HashTable_Free(table, &Test_HashTable::InstrumentedVerifiedFree);
  ASSERT_EQ(kNumKeys, freeInvocations_);
  HW1Environment::AddPoints(20);
}

TEST_F(Test_HashTable, Iterator_Removal_FirstElementOfChain) {
  static const int kTableSize = 3;

  HW1Environment::OpenTestCase();

  // Create a table with two elements in the middleth chain.
  HashTable *ht = HashTable_Allocate(kTableSize);
  InsertElement(ht, 1);
  InsertElement(ht, 1 + kTableSize);
  LinkedList *pl = ht->buckets[1];
  ASSERT_EQ(2, LinkedList_NumElements(pl));

  // Create an iterator pointing at the first element.  Recall that students
  // have both a Push() and an Append() method for modifying a bucket's list,
  // so we don't know which element is first in the list.
  HTIterator *it = HTIterator_Allocate(ht);
  ASSERT_TRUE(HTIterator_IsValid(it));

  HTKeyValue_t oldkv;
  ASSERT_TRUE(HTIterator_Get(it, &oldkv));
  HTKey_t removed = oldkv.key;
  HTKey_t remaining;
  if (removed == 1) {
    remaining = removed + kTableSize;
  } else {
    remaining = 1;
  }

  // Now, do the deletion.
  Reset(&oldkv);
  ASSERT_TRUE(HTIterator_Remove(it, &oldkv));
  ASSERT_EQ(removed, oldkv.key);
  ASSERT_EQ(removed, AsKeyType(oldkv.value));
  FreeValue(oldkv.value);
  HW1Environment::AddPoints(5);

  // Verify that the linked list is still good.
  ASSERT_EQ(1, LinkedList_NumElements(pl));
  HTKeyValue_t *remaining_kv = static_cast<HTKeyValue_t*>(pl->head->payload);
  ASSERT_EQ(remaining, remaining_kv->key);

  // Verify that the iterator is still good.
  ASSERT_TRUE(HTIterator_IsValid(it));
  Reset(&oldkv);
  ASSERT_TRUE(HTIterator_Get(it, &oldkv));
  ASSERT_EQ(remaining, oldkv.key);

  HashTable_Free(ht, &Test_HashTable::VerifiedFree);
  HTIterator_Free(it);
  HW1Environment::AddPoints(5);
}

TEST_F(Test_HashTable, Resize) {
  static const int kInitialNumBuckets = 2;
  static const int kFinalNumElements = 100;

  HW1Environment::OpenTestCase();

  HashTable *table = HashTable_Allocate(kInitialNumBuckets);
  ASSERT_EQ(kInitialNumBuckets, table->num_buckets);

  // Add enough elements to the table to force a resize.
  for (int i = 0; i < kFinalNumElements; ++i) {
    InsertElement(table, i);
  }
  ASSERT_LT(kInitialNumBuckets, table->num_buckets);
  HW1Environment::AddPoints(5);

  // Make sure that all of the elements are still inside the
  // HashTable after its resize.
  int num_times_seen[kFinalNumElements] = { 0 };   // array of 0's
  HTKeyValue_t oldkv;
  for (int i = 0; i < kFinalNumElements; ++i) {
    Reset(&oldkv);
    ASSERT_TRUE(HashTable_Find(table, i, &oldkv));
    ASSERT_EQ(static_cast<HTKey_t>(i), oldkv.key);
    ASSERT_EQ(static_cast<HTKey_t>(i), AsKeyType(oldkv.value));

    num_times_seen[i]++;
    ASSERT_EQ(1, num_times_seen[i]);
  }

  HashTable_Free(table, &Test_HashTable::VerifiedFree);
  HW1Environment::AddPoints(5);
}

}  // namespace hw1
