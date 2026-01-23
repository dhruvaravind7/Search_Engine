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

#include <stdio.h>
#include <stdlib.h>

extern "C" {
  #include "./FileParser.h"
}

#include "gtest/gtest.h"
#include "./test_suite.h"

namespace hw2 {

TEST(Test_FileParser, ReadFileToString) {
  HW2Environment::OpenTestCase();
  char* file_buf = NULL;
  int len = 0;

  // Read a non-trivial file.
  file_buf = ReadFileToString(
      const_cast<char*>("./test_tree/books/aliceinwonderland.txt"), &len);
  ASSERT_NE(static_cast<char*>(NULL), file_buf);
  ASSERT_EQ(163763, len);
  ASSERT_EQ('P', file_buf[0]);
  ASSERT_EQ('s', file_buf[163760]);
  free(file_buf);
  HW2Environment::AddPoints(10);

  // Try to read a non-existent file.
  file_buf = ReadFileToString(const_cast<char*>("bogus/file"), &len);
  ASSERT_EQ(static_cast<char*>(NULL), file_buf);
  HW2Environment::AddPoints(5);

  // Read a small file.
  file_buf = ReadFileToString(const_cast<char*>("./test_tree/bash-4.2/trap.h"),
                             &len);
  ASSERT_NE(static_cast<char*>(NULL), file_buf);
  ASSERT_EQ(3213, len);
  ASSERT_EQ('/', file_buf[0]);
  ASSERT_EQ('\n', file_buf[3212]);
  free(file_buf);
  HW2Environment::AddPoints(5);
}

TEST(Test_FileParser, ParseIntoWordPositionsTable_Simple) {
  HW2Environment::OpenTestCase();

  // Leave this string on the heap, since ParseIntoWordPositionsTable takes
  // ownership of its input
  // https://en.wikipedia.org/wiki/James_while_John_had_had_had_had_had_had_had_had_had_had_had_a_better_effect_on_the_teacher
  const char kString[] = "James while John had had had had had had had had had"
    " had had a better effect on the teacher";
  char* file_buf = static_cast<char*>(malloc(strlen(kString) + 1));
  strcpy(file_buf, kString);

  HashTable* tab = ParseIntoWordPositionsTable(file_buf);
  ASSERT_NE(static_cast<HashTable*>(NULL), tab);

  ASSERT_EQ(10, HashTable_NumElements(tab));
  HW2Environment::AddPoints(5);

  HTKeyValue_t kv;
  WordPositions *wp;
  LLPayload_t pos;

  static const char *kJames = "james";
  ASSERT_TRUE(HashTable_Find(tab,
                             FNVHash64((unsigned char*) kJames, strlen(kJames)),
                             &kv));
  wp = static_cast<WordPositions*>(kv.value);
  ASSERT_STREQ(kJames, wp->word);
  ASSERT_EQ(1, LinkedList_NumElements(wp->positions));
  ASSERT_TRUE(LinkedList_Pop(wp->positions, &pos));
  ASSERT_EQ(reinterpret_cast<LLPayload_t>(0U), pos);
  HW2Environment::AddPoints(5);

  static const char *kHad = "had";
  ASSERT_TRUE(HashTable_Find(tab,
                             FNVHash64((unsigned char*) kHad, strlen(kHad)),
                             &kv));
  wp = static_cast<WordPositions*>(kv.value);
  ASSERT_STREQ(kHad, wp->word);
  ASSERT_EQ(11, LinkedList_NumElements(wp->positions));
  ASSERT_TRUE(LinkedList_Pop(wp->positions, &pos));
  ASSERT_EQ(reinterpret_cast<LLPayload_t>(17U), pos);
  HW2Environment::AddPoints(5);


  ASSERT_FALSE(HashTable_Find(tab,
                              FNVHash64((unsigned char*) "nope", 4),
                              &kv));
  HW2Environment::AddPoints(5);

  FreeWordPositionsTable(tab);
  HW2Environment::AddPoints(5);
}

TEST(Test_FileParser, ParseIntoWordPositionsTable_Large) {
  HW2Environment::OpenTestCase();
  HashTable* tab;
  int num_bytes;
  char* file_buf = ReadFileToString(
      const_cast<char*>("./test_tree/books/lesmiserables.txt"),
      &num_bytes);
  ASSERT_NE(static_cast<char*>(NULL), file_buf);
  ASSERT_LT(0, num_bytes);

  tab = ParseIntoWordPositionsTable(file_buf);
  ASSERT_NE(static_cast<HashTable*>(NULL), tab);

  ASSERT_EQ(22876, HashTable_NumElements(tab));
  FreeWordPositionsTable(tab);
  HW2Environment::AddPoints(10);
}

TEST(Test_FileParser, ParseIntoWordPositionsTable_NotAscii) {
  HW2Environment::OpenTestCase();

  HashTable* tab;
  int num_bytes;
  char* file_buf = ReadFileToString(
      const_cast<char*>("./test_tree/bash-4.2/doc/article.pdf"),
      &num_bytes);
  ASSERT_NE(static_cast<char*>(NULL), file_buf);
  ASSERT_LT(0, num_bytes);

  tab = ParseIntoWordPositionsTable(file_buf);
  ASSERT_EQ(static_cast<HashTable*>(NULL), tab);

  HW2Environment::AddPoints(10);
}

}  // namespace hw2
