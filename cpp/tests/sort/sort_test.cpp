/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cudf_test/base_fixture.hpp>
#include <cudf_test/column_utilities.hpp>
#include <cudf_test/column_wrapper.hpp>
#include <cudf_test/table_utilities.hpp>
#include <cudf_test/type_lists.hpp>

#include <cudf/column/column_factories.hpp>
#include <cudf/copying.hpp>
#include <cudf/fixed_point/fixed_point.hpp>
#include <cudf/sorting.hpp>
#include <cudf/table/table_view.hpp>
#include <cudf/types.hpp>
#include <cudf/utilities/type_dispatcher.hpp>

#include <vector>

namespace cudf {
namespace test {
void run_sort_test(table_view input,
                   column_view expected_sorted_indices,
                   std::vector<order> column_order         = {},
                   std::vector<null_order> null_precedence = {})
{
  // Sorted table
  auto got_sorted_table      = sort(input, column_order, null_precedence);
  auto expected_sorted_table = gather(input, expected_sorted_indices);

  CUDF_TEST_EXPECT_TABLES_EQUAL(expected_sorted_table->view(), got_sorted_table->view());

  // Sorted by key
  auto got_sort_by_key_table      = sort_by_key(input, input, column_order, null_precedence);
  auto expected_sort_by_key_table = gather(input, expected_sorted_indices);

  CUDF_TEST_EXPECT_TABLES_EQUAL(expected_sort_by_key_table->view(), got_sort_by_key_table->view());
}

using TestTypes = cudf::test::Concat<cudf::test::IntegralTypesNotBool,
                                     cudf::test::FloatingPointTypes,
                                     cudf::test::DurationTypes,
                                     cudf::test::TimestampTypes>;

template <typename T>
struct Sort : public BaseFixture {
};

TYPED_TEST_CASE(Sort, TestTypes);

TYPED_TEST(Sort, WithNullMax)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8, 5}, {1, 1, 0, 1, 1, 1}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k", "d"}, {1, 1, 0, 1, 1, 1});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2, 10}, {1, 1, 0, 1, 1, 1}};
  table_view input{{col1, col2, col3}};

  fixed_width_column_wrapper<int32_t> expected{{1, 0, 5, 3, 4, 2}};
  std::vector<order> column_order{order::ASCENDING, order::ASCENDING, order::DESCENDING};
  std::vector<null_order> null_precedence{null_order::AFTER, null_order::AFTER, null_order::AFTER};

  // Sorted order
  auto got = sorted_order(input, column_order, null_precedence);

  if (!std::is_same_v<T, bool>) {
    CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

    // Run test for sort and sort_by_key
    run_sort_test(input, expected, column_order, null_precedence);
  } else {
    // for bools only validate that the null element landed at the back, since
    // the rest of the values are equivalent and yields random sorted order.
    auto to_host = [](column_view const& col) {
      thrust::host_vector<int32_t> h_data(col.size());
      CUDA_TRY(cudaMemcpy(
        h_data.data(), col.data<int32_t>(), h_data.size() * sizeof(int32_t), cudaMemcpyDefault));
      return h_data;
    };
    thrust::host_vector<int32_t> h_exp = to_host(expected);
    thrust::host_vector<int32_t> h_got = to_host(got->view());
    EXPECT_EQ(h_exp[h_exp.size() - 1], h_got[h_got.size() - 1]);

    // Run test for sort and sort_by_key
    fixed_width_column_wrapper<int32_t> expected_for_bool{{0, 3, 5, 1, 4, 2}};
    run_sort_test(input, expected_for_bool, column_order, null_precedence);
  }
}

TYPED_TEST(Sort, WithNullMin)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8}, {1, 1, 0, 1, 1}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k"}, {1, 1, 0, 1, 1});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2}, {1, 1, 0, 1, 1}};
  table_view input{{col1, col2, col3}};

  fixed_width_column_wrapper<int32_t> expected{{2, 1, 0, 3, 4}};
  std::vector<order> column_order{order::ASCENDING, order::ASCENDING, order::DESCENDING};

  auto got = sorted_order(input, column_order);

  if (!std::is_same_v<T, bool>) {
    CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

    // Run test for sort and sort_by_key
    run_sort_test(input, expected, column_order);
  } else {
    // for bools only validate that the null element landed at the front, since
    // the rest of the values are equivalent and yields random sorted order.
    auto to_host = [](column_view const& col) {
      thrust::host_vector<int32_t> h_data(col.size());
      CUDA_TRY(cudaMemcpy(
        h_data.data(), col.data<int32_t>(), h_data.size() * sizeof(int32_t), cudaMemcpyDefault));
      return h_data;
    };
    thrust::host_vector<int32_t> h_exp = to_host(expected);
    thrust::host_vector<int32_t> h_got = to_host(got->view());
    EXPECT_EQ(h_exp.front(), h_got.front());

    // Run test for sort and sort_by_key
    fixed_width_column_wrapper<int32_t> expected_for_bool{{2, 0, 3, 1, 4}};
    run_sort_test(input, expected_for_bool, column_order);
  }
}

TYPED_TEST(Sort, WithMixedNullOrder)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8}, {0, 0, 1, 1, 0}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k"}, {0, 1, 0, 0, 1});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2}, {1, 0, 1, 0, 1}};
  table_view input{{col1, col2, col3}};

  fixed_width_column_wrapper<int32_t> expected{{2, 3, 0, 1, 4}};
  std::vector<order> column_order{order::ASCENDING, order::ASCENDING, order::ASCENDING};
  std::vector<null_order> null_precedence{null_order::AFTER, null_order::BEFORE, null_order::AFTER};

  auto got = sorted_order(input, column_order, null_precedence);

  if (!std::is_same_v<T, bool>) {
    CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());
  } else {
    // for bools only validate that the null element landed at the front, since
    // the rest of the values are equivalent and yields random sorted order.
    auto to_host = [](column_view const& col) {
      thrust::host_vector<int32_t> h_data(col.size());
      CUDA_TRY(cudaMemcpy(
        h_data.data(), col.data<int32_t>(), h_data.size() * sizeof(int32_t), cudaMemcpyDefault));
      return h_data;
    };
    thrust::host_vector<int32_t> h_exp = to_host(expected);
    thrust::host_vector<int32_t> h_got = to_host(got->view());
    EXPECT_EQ(h_exp.front(), h_got.front());
  }

  // Run test for sort and sort_by_key
  run_sort_test(input, expected, column_order, null_precedence);
}

TYPED_TEST(Sort, WithAllValid)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k"});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2}};
  table_view input{{col1, col2, col3}};

  fixed_width_column_wrapper<int32_t> expected{{2, 1, 0, 3, 4}};
  std::vector<order> column_order{order::ASCENDING, order::ASCENDING, order::DESCENDING};

  auto got = sorted_order(input, column_order);

  // Skip validating bools order. Valid true bools are all
  // equivalent, and yield random order after thrust::sort
  if (!std::is_same_v<T, bool>) {
    CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

    // Run test for sort and sort_by_key
    run_sort_test(input, expected, column_order);
  } else {
    // Run test for sort and sort_by_key
    fixed_width_column_wrapper<int32_t> expected_for_bool{{2, 0, 3, 1, 4}};
    run_sort_test(input, expected_for_bool, column_order);
  }
}

TYPED_TEST(Sort, WithStructColumn)
{
  using T = TypeParam;

  std::initializer_list<std::string> names = {"Samuel Vimes",
                                              "Carrot Ironfoundersson",
                                              "Angua von Überwald",
                                              "Cheery Littlebottom",
                                              "Detritus",
                                              "Mr Slant"};
  auto num_rows{std::distance(names.begin(), names.end())};
  auto names_col = cudf::test::strings_column_wrapper{names.begin(), names.end()};
  auto ages_col  = cudf::test::fixed_width_column_wrapper<T, int32_t>{{48, 27, 25, 31, 351, 351}};

  auto is_human_col = cudf::test::fixed_width_column_wrapper<bool>{
    {true, true, false, false, false, false}, {1, 1, 0, 1, 1, 0}};

  auto struct_col =
    cudf::test::structs_column_wrapper{{names_col, ages_col, is_human_col}}.release();
  auto struct_col_view{struct_col->view()};
  EXPECT_EQ(num_rows, struct_col->size());

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8, 9}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k", "a"});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2, 20}};
  table_view input{{col1, col2, col3, struct_col_view}};

  fixed_width_column_wrapper<int32_t> expected{{2, 1, 0, 3, 4, 5}};
  std::vector<order> column_order{
    order::ASCENDING, order::ASCENDING, order::DESCENDING, order::ASCENDING};

  auto got = sorted_order(input, column_order);

  // Skip validating bools order. Valid true bools are all
  // equivalent, and yield random order after thrust::sort
  if (!std::is_same_v<T, bool>) {
    CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

    // Run test for sort and sort_by_key
    run_sort_test(input, expected, column_order);
  } else {
    // Run test for sort and sort_by_key
    fixed_width_column_wrapper<int32_t> expected_for_bool{{2, 5, 3, 0, 1, 4}};
    run_sort_test(input, expected_for_bool, column_order);
  }
}

TYPED_TEST(Sort, WithNestedStructColumn)
{
  using T = TypeParam;

  std::initializer_list<std::string> names = {"Samuel Vimes",
                                              "Carrot Ironfoundersson",
                                              "Angua von Überwald",
                                              "Cheery Littlebottom",
                                              "Detritus",
                                              "Mr Slant"};
  std::vector<bool> v{1, 1, 0, 1, 1, 0};
  auto names_col = cudf::test::strings_column_wrapper{names.begin(), names.end()};
  auto ages_col  = cudf::test::fixed_width_column_wrapper<T, int32_t>{{48, 27, 25, 31, 351, 351}};
  auto is_human_col = cudf::test::fixed_width_column_wrapper<bool>{
    {true, true, false, false, false, false}, {1, 1, 0, 1, 1, 0}};
  auto struct_col1 = cudf::test::structs_column_wrapper{{names_col, ages_col, is_human_col}, v};

  auto ages_col2   = cudf::test::fixed_width_column_wrapper<T, int32_t>{{48, 27, 25, 31, 351, 351}};
  auto struct_col2 = cudf::test::structs_column_wrapper{{ages_col2, struct_col1}}.release();

  auto struct_col_view{struct_col2->view()};

  fixed_width_column_wrapper<T> col1{{6, 6, 6, 6, 6, 6}};
  fixed_width_column_wrapper<T> col2{{1, 1, 1, 2, 2, 2}};
  table_view input{{col1, col2, struct_col_view}};

  fixed_width_column_wrapper<int32_t> expected{{3, 5, 4, 2, 1, 0}};
  std::vector<order> column_order{order::ASCENDING, order::DESCENDING, order::ASCENDING};

  auto got = sorted_order(input, column_order);

  // Skip validating bools order. Valid true bools are all
  // equivalent, and yield random order after thrust::sort
  if (!std::is_same_v<T, bool>) {
    CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

    // Run test for sort and sort_by_key
    run_sort_test(input, expected, column_order);
  } else {
    // Run test for sort and sort_by_key
    fixed_width_column_wrapper<int32_t> expected_for_bool{{2, 5, 1, 3, 4, 0}};
    run_sort_test(input, expected_for_bool, column_order);
  }
}

TYPED_TEST(Sort, WithSingleStructColumn)
{
  using T = TypeParam;

  std::initializer_list<std::string> names = {"Samuel Vimes",
                                              "Carrot Ironfoundersson",
                                              "Angua von Überwald",
                                              "Cheery Littlebottom",
                                              "Detritus",
                                              "Mr Slant"};
  std::vector<bool> v{1, 1, 0, 1, 1, 0};
  auto names_col = cudf::test::strings_column_wrapper{names.begin(), names.end()};
  auto ages_col  = cudf::test::fixed_width_column_wrapper<T, int32_t>{{48, 27, 25, 31, 351, 351}};
  auto is_human_col = cudf::test::fixed_width_column_wrapper<bool>{
    {true, true, false, false, false, false}, {1, 1, 0, 1, 1, 0}};
  auto struct_col =
    cudf::test::structs_column_wrapper{{names_col, ages_col, is_human_col}, v}.release();
  auto struct_col_view{struct_col->view()};
  table_view input{{struct_col_view}};

  fixed_width_column_wrapper<int32_t> expected{{2, 5, 1, 3, 4, 0}};
  std::vector<order> column_order{order::ASCENDING};

  auto got = sorted_order(input, column_order);

  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

  // Run test for sort and sort_by_key
  run_sort_test(input, expected, column_order);
}

TYPED_TEST(Sort, WithSlicedStructColumn)
{
  using T = TypeParam;
  /*
       /+-------------+
       |             s|
       +--------------+
     0 | {"bbe", 1, 7}|
     1 | {"bbe", 1, 8}|
     2 | {"aaa", 0, 1}|
     3 | {"abc", 0, 1}|
     4 | {"ab",  0, 9}|
     5 | {"za",  2, 5}|
     6 | {"b",   1, 7}|
     7 | { @,    3, 3}|
       +--------------+
  */
  // clang-format off
  using FWCW = cudf::test::fixed_width_column_wrapper<T, int32_t>;
  std::vector<bool>             string_valids{    1,     1,     1,     1,    1,    1,   1,   0};
  std::initializer_list<std::string> names = {"bbe", "bbe", "aaa", "abc", "ab", "za", "b", "x"};
  auto col2 =                           FWCW{{    1,     1,     0,     0,    0,    2,   1,   3}};
  auto col3 =                           FWCW{{    7,     8,     1,     1,    9,    5,   7,   3}};
  auto col1 = cudf::test::strings_column_wrapper{names.begin(), names.end(), string_valids.begin()};
  auto struct_col = structs_column_wrapper{{col1, col2, col3}}.release();
  // clang-format on
  auto struct_col_view{struct_col->view()};
  table_view input{{struct_col_view}};
  auto sliced_columns = cudf::split(struct_col_view, std::vector<size_type>{3});
  auto sliced_tables  = cudf::split(input, std::vector<size_type>{3});
  std::vector<order> column_order{order::ASCENDING};
  /*
        asce_null_first   sliced[3:]
      /+-------------+
      |             s|
      +--------------+
    7 | { @,    3, 3}|   7=4
    2 | {"aaa", 0, 1}|
    4 | {"ab",  0, 9}|   4=1
    3 | {"abc", 0, 1}|   3=0
    6 | {"b",   1, 7}|   6=3
    0 | {"bbe", 1, 7}|
    1 | {"bbe", 1, 8}|
    5 | {"za",  2, 5}|   5=2
      +--------------+
  */

  // normal
  fixed_width_column_wrapper<int32_t> expected{{7, 2, 4, 3, 6, 0, 1, 5}};
  auto got = sorted_order(input, column_order);
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected, column_order);

  // table with sliced column
  table_view input2{{sliced_columns[1]}};
  fixed_width_column_wrapper<int32_t> expected2{{4, 1, 0, 3, 2}};
  got = sorted_order(input2, column_order);
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected2, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input2, expected2, column_order);

  // sliced table[1]
  fixed_width_column_wrapper<int32_t> expected3{{4, 1, 0, 3, 2}};
  got = sorted_order(sliced_tables[1], column_order);
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected3, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(sliced_tables[1], expected3, column_order);

  // sliced table[0]
  fixed_width_column_wrapper<int32_t> expected4{{2, 0, 1}};
  got = sorted_order(sliced_tables[0], column_order);
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected4, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(sliced_tables[0], expected4, column_order);
}

TYPED_TEST(Sort, SlicedColumns)
{
  using T    = TypeParam;
  using FWCW = cudf::test::fixed_width_column_wrapper<T, int32_t>;

  // clang-format off
  std::vector<bool>             string_valids{    1,     1,     1,     1,    1,    1,   1,   0};
  std::initializer_list<std::string> names = {"bbe", "bbe", "aaa", "abc", "ab", "za", "b", "x"};
  auto col2 =                           FWCW{{    7,     8,     1,     1,    9,    5,   7,   3}};
  auto col1 = cudf::test::strings_column_wrapper{names.begin(), names.end(), string_valids.begin()};
  // clang-format on
  table_view input{{col1, col2}};
  auto sliced_columns1 = cudf::split(col1, std::vector<size_type>{3});
  auto sliced_columns2 = cudf::split(col1, std::vector<size_type>{3});
  auto sliced_tables   = cudf::split(input, std::vector<size_type>{3});
  std::vector<order> column_order{order::ASCENDING, order::ASCENDING};

  // normal
  // fixed_width_column_wrapper<int32_t> expected{{2, 3, 7, 5, 0, 6, 1, 4}};
  fixed_width_column_wrapper<int32_t> expected{{7, 2, 4, 3, 6, 0, 1, 5}};
  auto got = sorted_order(input, column_order);
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected, column_order);

  // table with sliced column
  table_view input2{{sliced_columns1[1], sliced_columns2[1]}};
  // fixed_width_column_wrapper<int32_t> expected2{{0, 4, 2, 3, 1}};
  fixed_width_column_wrapper<int32_t> expected2{{4, 1, 0, 3, 2}};
  got = sorted_order(input2, column_order);
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected2, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input2, expected2, column_order);
}

TYPED_TEST(Sort, WithStructColumnCombinations)
{
  using T    = TypeParam;
  using FWCW = cudf::test::fixed_width_column_wrapper<T, int32_t>;

  // clang-format off
  /*
    +------------+
    |           s|
    +------------+
  0 |   {0, null}|
  1 |   {1, null}|
  2 |        null|
  3 |{null, null}|
  4 |        null|
  5 |{null, null}|
  6 |   {null, 1}|
  7 |   {null, 0}|
    +------------+
  */
  std::vector<bool>                           struct_valids{1, 1, 0, 1, 0, 1, 1, 1};
  auto col1       = FWCW{{ 0,  1,  9, -1,  9, -1, -1, -1}, {1, 1, 1, 0, 1, 0, 0, 0}};
  auto col2       = FWCW{{-1, -1,  9, -1,  9, -1,  1,  0}, {0, 0, 1, 0, 1, 0, 1, 1}};
  auto struct_col = cudf::test::structs_column_wrapper{{col1, col2}, struct_valids}.release();
  /*
    desc_nulls_first     desc_nulls_last     asce_nulls_first     asce_nulls_last
    +------------+       +------------+      +------------+       +------------+
    |           s|       |           s|      |           s|       |           s|
    +------------+       +------------+      +------------+       +------------+
  2 |        null|     1 |   {1, null}|    2 |        null|     0 |   {0, null}|
  4 |        null|     0 |   {0, null}|    4 |        null|     1 |   {1, null}|
  3 |{null, null}|     6 |   {null, 1}|    3 |{null, null}|     7 |   {null, 0}|
  5 |{null, null}|     7 |   {null, 0}|    5 |{null, null}|     6 |   {null, 1}|
  6 |   {null, 1}|     3 |{null, null}|    7 |   {null, 0}|     3 |{null, null}|
  7 |   {null, 0}|     5 |{null, null}|    6 |   {null, 1}|     5 |{null, null}|
  1 |   {1, null}|     2 |        null|    0 |   {0, null}|     2 |        null|
  0 |   {0, null}|     4 |        null|    1 |   {1, null}|     4 |        null|
    +------------+       +------------+      +------------+       +------------+
  */
  // clang-format on
  auto struct_col_view{struct_col->view()};
  table_view input{{struct_col_view}};
  std::vector<order> column_order1{order::DESCENDING};

  // desc_nulls_first
  fixed_width_column_wrapper<int32_t> expected1{{2, 4, 3, 5, 6, 7, 1, 0}};
  auto got = sorted_order(input, column_order1, {null_order::AFTER});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected1, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected1, column_order1, {null_order::AFTER});

  // desc_nulls_last
  fixed_width_column_wrapper<int32_t> expected2{{1, 0, 6, 7, 3, 5, 2, 4}};
  got = sorted_order(input, column_order1, {null_order::BEFORE});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected2, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected2, column_order1, {null_order::BEFORE});

  // asce_nulls_first
  std::vector<order> column_order2{order::ASCENDING};
  fixed_width_column_wrapper<int32_t> expected3{{2, 4, 3, 5, 7, 6, 0, 1}};
  got = sorted_order(input, column_order2, {null_order::BEFORE});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected3, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected3, column_order2, {null_order::BEFORE});

  // asce_nulls_last
  fixed_width_column_wrapper<int32_t> expected4{{0, 1, 7, 6, 3, 5, 2, 4}};
  got = sorted_order(input, column_order2, {null_order::AFTER});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected4, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected4, column_order2, {null_order::AFTER});
}

TYPED_TEST(Sort, WithStructColumnCombinationsWithoutNulls)
{
  using T    = TypeParam;
  using FWCW = cudf::test::fixed_width_column_wrapper<T, int32_t>;

  // clang-format off
  /*
    +------------+
    |           s|
    +------------+
  0 |   {0, null}|
  1 |   {1, null}|
  2 |      {9, 9}|
  3 |{null, null}|
  4 |      {9, 9}|
  5 |{null, null}|
  6 |   {null, 1}|
  7 |   {null, 0}|
    +------------+
  */
  auto col1       = FWCW{{ 0,  1,  9, -1,  9, -1, -1, -1}, {1, 1, 1, 0, 1, 0, 0, 0}};
  auto col2       = FWCW{{-1, -1,  9, -1,  9, -1,  1,  0}, {0, 0, 1, 0, 1, 0, 1, 1}};
  auto struct_col = cudf::test::structs_column_wrapper{{col1, col2}}.release();
  /* (nested columns are always nulls_first, spark requirement)
    desc_nulls_*        asce_nulls_*
    +------------+      +------------+
    |           s|      |           s|
    +------------+      +------------+
  3 |{null, null}|    0 |   {0, null}|
  5 |{null, null}|    1 |   {1, null}|
  6 |   {null, 1}|    2 |      {9, 9}|
  7 |   {null, 0}|    4 |      {9, 9}|
  2 |      {9, 9}|    7 |   {null, 0}|
  4 |      {9, 9}|    6 |   {null, 1}|
  1 |   {1, null}|    3 |{null, null}|
  0 |   {0, null}|    5 |{null, null}|
    +------------+      +------------+
  */
  // clang-format on
  auto struct_col_view{struct_col->view()};
  table_view input{{struct_col_view}};
  std::vector<order> column_order{order::DESCENDING};

  // desc_nulls_first
  fixed_width_column_wrapper<int32_t> expected1{{3, 5, 6, 7, 2, 4, 1, 0}};
  auto got = sorted_order(input, column_order, {null_order::AFTER});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected1, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected1, column_order, {null_order::AFTER});

  // desc_nulls_last
  fixed_width_column_wrapper<int32_t> expected2{{2, 4, 1, 0, 6, 7, 3, 5}};
  got = sorted_order(input, column_order, {null_order::BEFORE});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected2, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected2, column_order, {null_order::BEFORE});

  // asce_nulls_first
  std::vector<order> column_order2{order::ASCENDING};
  fixed_width_column_wrapper<int32_t> expected3{{3, 5, 7, 6, 0, 1, 2, 4}};
  got = sorted_order(input, column_order2, {null_order::BEFORE});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected3, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected3, column_order2, {null_order::BEFORE});

  // asce_nulls_last
  fixed_width_column_wrapper<int32_t> expected4{{0, 1, 2, 4, 7, 6, 3, 5}};
  got = sorted_order(input, column_order2, {null_order::AFTER});
  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected4, got->view());
  // Run test for sort and sort_by_key
  run_sort_test(input, expected4, column_order2, {null_order::AFTER});
}

TYPED_TEST(Sort, Stable)
{
  using T = TypeParam;
  using R = int32_t;

  fixed_width_column_wrapper<T> col1({0, 1, 1, 0, 0, 1, 0, 1}, {0, 1, 1, 1, 1, 1, 1, 1});
  strings_column_wrapper col2({"2", "a", "b", "x", "k", "a", "x", "a"}, {1, 1, 1, 1, 0, 1, 1, 1});

  fixed_width_column_wrapper<R> expected{{4, 3, 6, 1, 5, 7, 2, 0}};

  auto got = stable_sorted_order(table_view({col1, col2}),
                                 {order::ASCENDING, order::ASCENDING},
                                 {null_order::AFTER, null_order::BEFORE});

  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());
}

TYPED_TEST(Sort, MisMatchInColumnOrderSize)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k"});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2}};
  table_view input{{col1, col2, col3}};

  std::vector<order> column_order{order::ASCENDING, order::DESCENDING};

  EXPECT_THROW(sorted_order(input, column_order), logic_error);
  EXPECT_THROW(stable_sorted_order(input, column_order), logic_error);
  EXPECT_THROW(sort(input, column_order), logic_error);
  EXPECT_THROW(sort_by_key(input, input, column_order), logic_error);
}

TYPED_TEST(Sort, MisMatchInNullPrecedenceSize)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k"});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2}};
  table_view input{{col1, col2, col3}};

  std::vector<order> column_order{order::ASCENDING, order::DESCENDING, order::DESCENDING};
  std::vector<null_order> null_precedence{null_order::AFTER, null_order::BEFORE};

  EXPECT_THROW(sorted_order(input, column_order, null_precedence), logic_error);
  EXPECT_THROW(stable_sorted_order(input, column_order, null_precedence), logic_error);
  EXPECT_THROW(sort(input, column_order, null_precedence), logic_error);
  EXPECT_THROW(sort_by_key(input, input, column_order, null_precedence), logic_error);
}

TYPED_TEST(Sort, ZeroSizedColumns)
{
  using T = TypeParam;

  fixed_width_column_wrapper<T> col1{};
  table_view input{{col1}};

  fixed_width_column_wrapper<int32_t> expected{};
  std::vector<order> column_order{order::ASCENDING};

  auto got = sorted_order(input, column_order);

  CUDF_TEST_EXPECT_COLUMNS_EQUAL(expected, got->view());

  // Run test for sort and sort_by_key
  run_sort_test(input, expected, column_order);
}

struct SortByKey : public BaseFixture {
};

TEST_F(SortByKey, ValueKeysSizeMismatch)
{
  using T = int64_t;

  fixed_width_column_wrapper<T> col1{{5, 4, 3, 5, 8}};
  strings_column_wrapper col2({"d", "e", "a", "d", "k"});
  fixed_width_column_wrapper<T> col3{{10, 40, 70, 5, 2}};
  table_view values{{col1, col2, col3}};

  fixed_width_column_wrapper<T> key_col{{5, 4, 3, 5}};
  table_view keys{{key_col}};

  EXPECT_THROW(sort_by_key(values, keys), logic_error);
}

template <typename T>
struct FixedPointTestBothReps : public cudf::test::BaseFixture {
};

template <typename T>
using wrapper = cudf::test::fixed_width_column_wrapper<T>;
TYPED_TEST_CASE(FixedPointTestBothReps, cudf::test::FixedPointTypes);

TYPED_TEST(FixedPointTestBothReps, FixedPointSortedOrderGather)
{
  using namespace numeric;
  using decimalXX = TypeParam;

  auto const ZERO  = decimalXX{0, scale_type{0}};
  auto const ONE   = decimalXX{1, scale_type{0}};
  auto const TWO   = decimalXX{2, scale_type{0}};
  auto const THREE = decimalXX{3, scale_type{0}};
  auto const FOUR  = decimalXX{4, scale_type{0}};

  auto const input_vec  = std::vector<decimalXX>{TWO, ONE, ZERO, FOUR, THREE};
  auto const index_vec  = std::vector<cudf::size_type>{2, 1, 0, 4, 3};
  auto const sorted_vec = std::vector<decimalXX>{ZERO, ONE, TWO, THREE, FOUR};

  auto const input_col  = wrapper<decimalXX>(input_vec.begin(), input_vec.end());
  auto const index_col  = wrapper<cudf::size_type>(index_vec.begin(), index_vec.end());
  auto const sorted_col = wrapper<decimalXX>(sorted_vec.begin(), sorted_vec.end());

  auto const sorted_table = cudf::table_view{{sorted_col}};
  auto const input_table  = cudf::table_view{{input_col}};

  auto const indices = cudf::sorted_order(input_table);
  auto const sorted  = cudf::gather(input_table, indices->view());

  CUDF_TEST_EXPECT_COLUMNS_EQUAL(index_col, indices->view());
  CUDF_TEST_EXPECT_TABLES_EQUAL(sorted_table, sorted->view());
}

}  // namespace test
}  // namespace cudf

CUDF_TEST_PROGRAM_MAIN()
