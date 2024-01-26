/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "helper_tests.h"

#include "helper.h"

#include "unity.h"

#include <stdio.h>
#include <time.h>

void test_interpolate_increasing()
{
    float a[] = { 1, 2, 3, 4, 5 };
    float b[] = { 2, 4, 6, 8, 10 };

    float value_b = interpolate(a, b, sizeof(a) / sizeof(float), 1.75);
    TEST_ASSERT_EQUAL_FLOAT(3.5, value_b);

    value_b = interpolate(a, b, sizeof(a) / sizeof(float), -1);
    TEST_ASSERT_EQUAL_FLOAT(2.0, value_b);

    value_b = interpolate(a, b, sizeof(a) / sizeof(float), 6);
    TEST_ASSERT_EQUAL_FLOAT(10.0, value_b);
}

void test_interpolate_decreasing()
{
    float a[] = { 5, 4, 3, 2, 1 };
    float b[] = { 2, 4, 6, 8, 10 };

    float value_b = interpolate(a, b, sizeof(a) / sizeof(float), 1.75);
    TEST_ASSERT_EQUAL_FLOAT(8.5, value_b);

    value_b = interpolate(a, b, sizeof(a) / sizeof(float), -1);
    TEST_ASSERT_EQUAL_FLOAT(10.0, value_b);

    value_b = interpolate(a, b, sizeof(a) / sizeof(float), 6);
    TEST_ASSERT_EQUAL_FLOAT(2.0, value_b);
}

void test_is_empty()
{
    float a[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    TEST_ASSERT_EQUAL(1, is_empty((uint8_t *)a, sizeof(a)));
}

void test_is_filled_last_element()
{
    float a[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0001 };
    TEST_ASSERT_EQUAL(0, is_empty((uint8_t *)a, sizeof(a)));
}

void test_is_filled_first_element()
{
    float a[] = { 0.0001, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    TEST_ASSERT_EQUAL(0, is_empty((uint8_t *)a, sizeof(a)));
}

void test_is_empty_errors()
{
    uint8_t to_small[] = { 1 };

    TEST_ASSERT_EQUAL(-EINVAL, is_empty((uint8_t *)to_small, sizeof(to_small)));
}

int helper_tests()
{
    UNITY_BEGIN();

    // RUN_TEST(test_interpolate_increasing);
    // RUN_TEST(test_interpolate_decreasing);
    RUN_TEST(test_is_empty);
    RUN_TEST(test_is_filled_last_element);
    RUN_TEST(test_is_filled_first_element);
    RUN_TEST(test_is_empty_errors);

    return UNITY_END();
}
