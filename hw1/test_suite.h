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

#ifndef HW1_TEST_SUITE_H_
#define HW1_TEST_SUITE_H_

#include "gtest/gtest.h"

class HW1Environment : public ::testing::Environment {
 public:
  static void AddPoints(int points);
  static void OpenTestCase();

  // These are run once for the entire test environment:
  virtual void SetUp();
  virtual void TearDown();

 private:
  static int total_points_;
  static int curr_test_points_;

  static constexpr int HW1_MAXPOINTS = 260;
};


#endif  // HW1_TEST_SUITE_H_
