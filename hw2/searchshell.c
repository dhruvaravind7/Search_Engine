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

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "libhw1/CSE333.h"
#include "./CrawlFileTree.h"
#include "./DocTable.h"
#include "./MemIndex.h"

//////////////////////////////////////////////////////////////////////////////
// Helper function declarations, constants, etc

//  Prints usage information and exits with failure
static void Usage(void);
//  Processes queries from stdin until EOF is reached, printing results
static void ProcessQueries(DocTable* dt, MemIndex* mi);
//  Reads the next line from the given file, returning it in *ret_str
static int GetNextLine(FILE* f, char** ret_str);


//////////////////////////////////////////////////////////////////////////////
// Main
int main(int argc, char** argv) {
  if (argc != 2) {
    Usage();
  }

  // Implement searchshell!  We're giving you very few hints
  // on how to do it, so you'll need to figure out an appropriate
  // decomposition into functions as well as implementing the
  // functions.  There are several major tasks you need to build:
  //
  //  - Crawl from a directory provided by argv[1] to produce and index
  //  - Prompt the user for a query and read the query from stdin, in a loop
  //  - Split a query into words (check out strtok_r)
  //  - Process a query against the index and print out the results
  //
  // When searchshell detects end-of-file on stdin (cntrl-D from the
  // keyboard), searchshell should free all dynamically allocated
  // memory and any other allocated resources and then exit.
  //
  // Note that you should make sure the fomatting of your
  // searchshell output exactly matches our solution binaries
  // to get full points on this part.
  DocTable* dt = NULL;
  MemIndex* mi = NULL;

  // Crawl the directory and build the index
  printf("Indexing '%s'\n", argv[1]);
  if (!CrawlFileTree(argv[1], &dt, &mi)) {
    fprintf(stderr, "Failed to crawl directory '%s'\n", argv[1]);
    Usage();
  }

  // Process queries
  ProcessQueries(dt, mi);

  // Clean up
  DocTable_Free(dt);
  MemIndex_Free(mi);
  return EXIT_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static void Usage(void) {
  fprintf(stderr, "Usage: ./searchshell <docroot>\n");
  fprintf(stderr,
          "where <docroot> is an absolute or relative " \
          "path to a directory to build an index under.\n");
  exit(EXIT_FAILURE);
}

static void ProcessQueries(DocTable* dt, MemIndex* mi) {
  char* query_line = NULL;

  while (1) {
    printf("enter query:\n");

    // Read the next line from stdin
    int result = GetNextLine(stdin, &query_line);
    if (result == -1) {
      // End of file
      printf("shutting down...\n");
      break;
    }

    // Split the query into words
    char** words = NULL;
    int num_words = 0;
    int words_capacity = 4;
    words = (char**) malloc(words_capacity * sizeof(char*));
    Verify333(words != NULL);

    char* saveptr;
    char* word = strtok_r(query_line, " \t", &saveptr);

    while (word != NULL) {
      // Convert to lowercase
      for (int i = 0; word[i] != '\0'; i++) {
        word[i] = tolower(word[i]);
      }

      // Expand array if needed
      if (num_words >= words_capacity) {
        words_capacity *= 2;
        words = (char**) realloc(words, words_capacity * sizeof(char*));
        Verify333(words != NULL);
      }

      words[num_words++] = word;
      word = strtok_r(NULL, " \t", &saveptr);
    }

    // Search the index
    if (num_words > 0) {
      LinkedList* results = MemIndex_Search(mi, words, num_words);

      if (results != NULL && LinkedList_NumElements(results) > 0) {
        // Print results
        LLIterator* it = LLIterator_Allocate(results);
        Verify333(it != NULL);

        while (LLIterator_IsValid(it)) {
          SearchResult* sr;
          LLIterator_Get(it, (LLPayload_t*)&sr);

          char* doc_name = DocTable_GetDocName(dt, sr->doc_id);
          printf("  %s (%d)\n", doc_name, sr->rank);

          LLIterator_Next(it);
        }
        LLIterator_Free(it);

        LinkedList_Free(results, free);
      }
      // If results is NULL or empty, print nothing
    }

    free(words);
    free(query_line);
  }
}

static int GetNextLine(FILE* f, char** ret_str) {
  size_t buffer_size = 128;
  char* buffer = (char*) malloc(buffer_size);
  if (buffer == NULL) {
    return -1;
  }

  size_t pos = 0;

  while (1) {
    int c = fgetc(f);

    if (c == EOF) {
      if (pos == 0) {
        free(buffer);
        return -1;
      }
      buffer[pos] = '\0';
      *ret_str = buffer;
      return pos;
    }

    if (c == '\n') {
      buffer[pos] = '\0';
      *ret_str = buffer;
      return pos;
    }

    buffer[pos++] = c;

    if (pos >= buffer_size - 1) {
      buffer_size *= 2;
      buffer = (char*) realloc(buffer, buffer_size);
      if (buffer == NULL) {
        return -1;
      }
    }
  }
}
