# Project Implementation Summary

- Implementation of all weeks has been completed

## Week 11

- Tests for TCP Socket:
  - Created a new file `tcp_test_utils.c` as a utility file for creating tests 
    - Corresponding header file: `tcp_test_utils.h`
  - Implemented `tcp-test-server.c` and `tcp-test-client.c` (despite being used locally with loopback)
    - Both implemented with `tcp_read` or `tcp_send` wrapped in a while loop

## Week 12

- Created a unit test file:
  - `http_prot_test.c` which will be compiled with `($ make http_prot_test)`

