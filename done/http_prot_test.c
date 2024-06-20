#define IN_CS202_UNIT_TEST
#include "http_prot.h"
#include "util.h"
#include "error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include <string.h>

// useful strings 
#define JUNK "junk"
#define EMPTY_STRING ""

// for uri_test
#define SHORT_URI_STRING1 "/universal/resource/"
#define SHORT_URI_STRING2 "/universal"
#define LONG_URI_STRING "/universal/resource/identifier"

// for verb_test 
#define LONG_VERB_GET_STRING "GET / HTTP/1.1"
#define LONG_VERB_POST_STRING "POST / HTTP/1.1"
#define GET "GET"
#define POST "POST"

// for var_test
#define LONG_VAR_STRING "http://localhost:8000/imgfs/read?res=orig&img_id=mure.jpg"
#define LONG_VAR_STRING_MODIFIED_ADD_PARAM_BEFORE_QUESTION_MARK "http://localhost:8000/imgfs/added=nice/read?res=orig&img_id=mure.jpg"
#define VAR_PARAM_STRING_BEFORE_QUESTION_MARK "added"
#define VAR_VALUE_STRING_BEFORE_QUESTION_MARK "nice"
#define VAR_PARAM1_STRING "res"
#define VAR_VALUE1_STRING "orig"
#define VAR_PARAM2_STRING "img_id"
#define VAR_VALUE2_STRING "mure.jpg"

static void http_string_print(struct http_string* s){
  printf("HTTP string: %.*s\n", (int) s->len, s->val);
}
static void c_string_print(const char* s){
  printf("C string: %s\n", s);
}
static void construct_http_string(const char* s, struct http_string* out){
  size_t len = strlen(s);
  out->len = len;
  out->val = malloc(len);
  assert(out->val != NULL);
  strncpy((char*) out->val, s, len);
}
static void destruct_http_string(struct http_string* out){
  free_const_ptr(out->val);
  out->len = 0;
}

// TEST : http_match_uri 
// ==================================================
START_TEST(test_http_match_uri_trivial_cases){
  struct http_message msg;

  construct_http_string(LONG_URI_STRING, &msg.uri);
  int res1 = http_match_uri(&msg, SHORT_URI_STRING1);
  int res2 = http_match_uri(&msg, SHORT_URI_STRING2);
  ck_assert_int_eq(res1, 1);
  ck_assert_int_eq(res2, 1);
  destruct_http_string(&msg.uri);
  
} END_TEST

START_TEST(test_http_match_uri_null_params){
  struct http_message msg;
  
  construct_http_string(JUNK, &msg.uri);
  int res = http_match_uri(NULL, JUNK);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  res = http_match_uri(&msg, NULL);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  res = http_match_uri(NULL, NULL);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  destruct_http_string(&msg.uri);
} END_TEST

START_TEST(test_http_match_uri_empty_uri){
  struct http_message msg;

  construct_http_string(EMPTY_STRING, &msg.uri);
  int res = http_match_uri(&msg, JUNK);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&msg.uri);
} END_TEST

START_TEST(test_http_match_uri_empty_target){
  struct http_message msg;

  construct_http_string(JUNK, &msg.uri);
  int res = http_match_uri(&msg, EMPTY_STRING);
  ck_assert_int_eq(res, 1);
  destruct_http_string(&msg.uri);
} END_TEST

START_TEST(test_http_match_uri_shorter_than_target){
  struct http_message msg;

  construct_http_string(SHORT_URI_STRING1, &msg.uri);
  int res = http_match_uri(&msg, LONG_URI_STRING);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&msg.uri);
} END_TEST


// TEST : http_match_verb
// ==================================================
START_TEST(test_http_match_verb_trivial_cases){
  struct http_string method;

  construct_http_string(LONG_VERB_POST_STRING, &method);
  int res = http_match_verb(&method, POST);
  ck_assert_int_eq(res, 0);
  method.len = strlen(POST);
  res = http_match_verb(&method, POST);
  ck_assert_int_eq(res, 1);
  destruct_http_string(&method);

  construct_http_string(LONG_VERB_GET_STRING, &method);
  res = http_match_verb(&method, GET);
  ck_assert_int_eq(res, 0);
  method.len = strlen(GET);
  res = http_match_verb(&method, GET);
  ck_assert_int_eq(res, 1);
  destruct_http_string(&method);
} END_TEST

START_TEST(test_http_match_verb_null_params){
  struct http_string method;

  construct_http_string(JUNK, &method);
  int res = http_match_verb(&method, NULL);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  res = http_match_verb(NULL, JUNK);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  res = http_match_verb(NULL, NULL);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  destruct_http_string(&method);
} END_TEST

START_TEST(test_http_match_verb_empty_method){
  struct http_string method;

  construct_http_string(EMPTY_STRING, &method);
  int res = http_match_verb(&method, JUNK);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&method);
} END_TEST

START_TEST(test_http_match_verb_empty_verb){
  struct http_string method;

  construct_http_string(JUNK, &method);
  int res = http_match_verb(&method, EMPTY_STRING);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&method);
} END_TEST

START_TEST(test_http_match_verb_method_shorter_than_verb){
  struct http_string method;

  construct_http_string(GET, &method);
  int res = http_match_verb(&method, LONG_VERB_GET_STRING);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&method);
} END_TEST


// TEST : http_get_var
// ==================================================
START_TEST(test_http_get_var_null_params){
  char* buf = NULL;
  struct http_string url;
  char* name = NULL;

  size_t buf_len = strlen(VAR_VALUE1_STRING);
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = VAR_PARAM1_STRING;
  construct_http_string(LONG_VAR_STRING, &url);
  int res = http_get_var(NULL, name, buf, buf_len + 1);
  res |= http_get_var(&url, NULL, buf, buf_len + 1);
  res |= http_get_var(&url, name, NULL, buf_len + 1);
  free(buf);
  ck_assert_int_eq(res, ERR_INVALID_ARGUMENT);
  destruct_http_string(&url);
} END_TEST

START_TEST(test_http_get_var_trivial_cases){
  char* buf = NULL;
  struct http_string url;
  char* name = NULL;

  construct_http_string(LONG_VAR_STRING, &url); //url is same of both cases below

  size_t buf_len = strlen(VAR_VALUE1_STRING); //buf will perfectly fit the value
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = VAR_PARAM1_STRING;
  int res = http_get_var(&url, name, buf, buf_len + 1);
  ck_assert_int_eq(res, buf_len);
  ck_assert_str_eq(buf, VAR_VALUE1_STRING);
  free(buf);

  buf_len = strlen(VAR_VALUE2_STRING); //buf will perfectly fit the value
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = VAR_PARAM2_STRING;
  res = http_get_var(&url, name, buf, buf_len + 1);
  ck_assert_int_eq(res, buf_len);
  ck_assert_str_eq(buf, VAR_VALUE2_STRING);
  free(buf);

  destruct_http_string(&url);
} END_TEST

START_TEST(test_http_get_var_url_shorter_than_name){
  char* buf = NULL;
  struct http_string url;
  char* name = NULL;

  size_t buf_len = strlen(VAR_VALUE1_STRING);
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = LONG_VAR_STRING;
  construct_http_string(JUNK, &url);
  int res = http_get_var(&url, name, buf, buf_len + 1);
  free(buf);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&url);
} END_TEST

START_TEST(test_http_get_var_name_not_found){
  char* buf = NULL;
  struct http_string url;
  char* name = NULL;

  size_t  buf_len = strlen(VAR_VALUE1_STRING);
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = JUNK;
  construct_http_string(LONG_VAR_STRING, &url);
  int res = http_get_var(&url, name, buf, buf_len + 1);
  free(buf);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&url);
} END_TEST

START_TEST(test_http_get_var_small_buffer_for_value){
  char* buf = NULL;
  struct http_string url;
  char* name = NULL;

  size_t  buf_len = strlen(VAR_VALUE1_STRING) / 2;
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = VAR_PARAM1_STRING;
  construct_http_string(LONG_VAR_STRING, &url);
  int res = http_get_var(&url, name, buf, buf_len + 1);
  free(buf);
  ck_assert_int_eq(res, ERR_RUNTIME);
  destruct_http_string(&url);
} END_TEST

START_TEST(test_http_get_var_param_before_question_mark){
  char* buf = NULL;
  struct http_string url;
  char* name = NULL;

  size_t buf_len = strlen(VAR_VALUE_STRING_BEFORE_QUESTION_MARK);
  buf = malloc(buf_len + 1);
  assert(buf != NULL);
  name = VAR_PARAM_STRING_BEFORE_QUESTION_MARK;
  construct_http_string(LONG_VAR_STRING_MODIFIED_ADD_PARAM_BEFORE_QUESTION_MARK, &url);
  int res = http_get_var(&url, name, buf, buf_len + 1);
  free(buf);
  ck_assert_int_eq(res, 0);
  destruct_http_string(&url);
} END_TEST


// TEST : get_next_token 
// ==================================================
START_TEST(test_get_next_token_trivial_cases){

} END_TEST

// TEST : test suite 
// Create the test suite and add test cases
Suite* http_uri_tests(void) {
    Suite *s = suite_create("HTTP URI Tests");
    TCase *tc_uri = tcase_create("URI Matching");

    // Add the test_http_match_uri_trivial_cases to the test case
    tcase_add_test(tc_uri, test_http_match_uri_trivial_cases);
    tcase_add_test(tc_uri, test_http_match_uri_null_params);
    tcase_add_test(tc_uri, test_http_match_uri_empty_uri);
    tcase_add_test(tc_uri, test_http_match_uri_empty_target);
    tcase_add_test(tc_uri, test_http_match_uri_shorter_than_target);
    suite_add_tcase(s, tc_uri);

    return s;
}
Suite* http_verb_tests(void) {
    Suite *s = suite_create("HTTP Verb Tests");
    TCase *tc_verb = tcase_create("Verb Matching");

    tcase_add_test(tc_verb, test_http_match_verb_trivial_cases);
    tcase_add_test(tc_verb, test_http_match_verb_null_params);
    tcase_add_test(tc_verb, test_http_match_verb_empty_method);
    tcase_add_test(tc_verb, test_http_match_verb_empty_verb);
    suite_add_tcase(s, tc_verb);

    return s;
}
Suite* http_var_tests(void) {
    Suite *s = suite_create("HTTP Get Var Tests");
    TCase *tc_verb = tcase_create("Get Var");

    tcase_add_test(tc_verb, test_http_get_var_null_params);
    tcase_add_test(tc_verb, test_http_get_var_trivial_cases);
    tcase_add_test(tc_verb, test_http_get_var_url_shorter_than_name);
    tcase_add_test(tc_verb, test_http_get_var_name_not_found);
    tcase_add_test(tc_verb, test_http_get_var_small_buffer_for_value);
    tcase_add_test(tc_verb, test_http_get_var_param_before_question_mark);
    suite_add_tcase(s, tc_verb);

    return s;
}

int main(void) {
    int number_failed = 0;
    Suite *s = http_uri_tests();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed += srunner_ntests_failed(sr);
    srunner_free(sr);

    s = http_verb_tests();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed += srunner_ntests_failed(sr);
    srunner_free(sr);

    s = http_var_tests();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed += srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
