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

#include <cudf/detail/aggregation/aggregation.hpp>
#include <cudf/utilities/traits.hpp>

#include <cudf_test/base_fixture.hpp>
#include <cudf_test/column_wrapper.hpp>
#include <cudf_test/iterator_utilities.hpp>
#include <cudf_test/type_list_utilities.hpp>
#include <cudf_test/type_lists.hpp>

#include <tests/groupby/groupby_test_util.hpp>

#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <vector>

using namespace cudf::test::iterators;

namespace cudf {
namespace test {
template <typename V>
struct groupby_mean_test : public cudf::test::BaseFixture {
};

template <typename Target, typename Source>
std::vector<Target> convert(std::initializer_list<Source> in)
{
  std::vector<Target> out(std::cbegin(in), std::cend(in));
  return out;
}

using supported_types =
  cudf::test::Concat<cudf::test::Types<int8_t, int16_t, int32_t, int64_t, float, double>,
                     cudf::test::DurationTypes>;
TYPED_TEST_CASE(groupby_mean_test, supported_types);
using K = int32_t;

TYPED_TEST(groupby_mean_test, basic)
{
  using V  = TypeParam;
  using R  = cudf::detail::target_type_t<V, aggregation::MEAN>;
  using RT = typename std::conditional<cudf::is_duration<R>(), int, double>::type;

  fixed_width_column_wrapper<K> keys{1, 2, 3, 1, 2, 2, 1, 3, 3, 2};
  fixed_width_column_wrapper<V> vals{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  // clang-format off
  fixed_width_column_wrapper<K> expect_keys{1,       2,          3};
  //                                       {0, 3, 6, 1, 4, 5, 9, 2, 7, 8}
  std::vector<RT> expect_v = convert<RT>(  {3.,      19. / 4,    17. / 3});
  fixed_width_column_wrapper<R, RT> expect_vals(expect_v.cbegin(), expect_v.cend());
  // clang-format on

  auto agg = cudf::make_mean_aggregation<groupby_aggregation>();
  test_single_agg(keys, vals, expect_keys, expect_vals, std::move(agg));
}

TYPED_TEST(groupby_mean_test, empty_cols)
{
  using V = TypeParam;
  using R = cudf::detail::target_type_t<V, aggregation::MEAN>;

  fixed_width_column_wrapper<K> keys{};
  fixed_width_column_wrapper<V> vals{};

  fixed_width_column_wrapper<K> expect_keys{};
  fixed_width_column_wrapper<R> expect_vals{};

  auto agg = cudf::make_mean_aggregation<groupby_aggregation>();
  test_single_agg(keys, vals, expect_keys, expect_vals, std::move(agg));
}

TYPED_TEST(groupby_mean_test, zero_valid_keys)
{
  using V = TypeParam;
  using R = cudf::detail::target_type_t<V, aggregation::MEAN>;

  fixed_width_column_wrapper<K> keys({1, 2, 3}, all_nulls());
  fixed_width_column_wrapper<V> vals{3, 4, 5};

  fixed_width_column_wrapper<K> expect_keys{};
  fixed_width_column_wrapper<R> expect_vals{};

  auto agg = cudf::make_mean_aggregation<groupby_aggregation>();
  test_single_agg(keys, vals, expect_keys, expect_vals, std::move(agg));
}

TYPED_TEST(groupby_mean_test, zero_valid_values)
{
  using V = TypeParam;
  using R = cudf::detail::target_type_t<V, aggregation::MEAN>;

  fixed_width_column_wrapper<K> keys{1, 1, 1};
  fixed_width_column_wrapper<V> vals({3, 4, 5}, all_nulls());

  fixed_width_column_wrapper<K> expect_keys{1};
  fixed_width_column_wrapper<R> expect_vals({0}, all_nulls());

  auto agg = cudf::make_mean_aggregation<groupby_aggregation>();
  test_single_agg(keys, vals, expect_keys, expect_vals, std::move(agg));
}

TYPED_TEST(groupby_mean_test, null_keys_and_values)
{
  using V  = TypeParam;
  using R  = cudf::detail::target_type_t<V, aggregation::MEAN>;
  using RT = typename std::conditional<cudf::is_duration<R>(), int, double>::type;

  fixed_width_column_wrapper<K> keys({1, 2, 3, 1, 2, 2, 1, 3, 3, 2, 4},
                                     {1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1});
  fixed_width_column_wrapper<V> vals({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 4},
                                     {0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0});

  // clang-format off
  //                                        {1, 1,     2, 2, 2,   3, 3,    4}
  fixed_width_column_wrapper<K> expect_keys({1,        2,         3,       4}, no_nulls());
  //                                        {3, 6,     1, 4, 9,   2, 8,    -}
  std::vector<RT> expect_v = convert<RT>(   {4.5,      14. / 3,   5.,      0.});
  fixed_width_column_wrapper<R, RT> expect_vals(expect_v.cbegin(), expect_v.cend(), {1, 1, 1, 0});
  // clang-format on

  auto agg = cudf::make_mean_aggregation<groupby_aggregation>();
  test_single_agg(keys, vals, expect_keys, expect_vals, std::move(agg));
}
// clang-format on

struct groupby_dictionary_mean_test : public cudf::test::BaseFixture {
};

TEST_F(groupby_dictionary_mean_test, basic)
{
  using V = int16_t;
  using R = cudf::detail::target_type_t<V, aggregation::MEAN>;

  // clang-format off
  fixed_width_column_wrapper<K> keys{1, 2, 3, 1, 2, 2, 1, 3, 3, 2};
  dictionary_column_wrapper<V>  vals{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  fixed_width_column_wrapper<K> expect_keys(        {1,      2,       3});
  fixed_width_column_wrapper<R, double> expect_vals({9. / 3, 19. / 4, 17. / 3});
  // clang-format on

  test_single_agg(
    keys, vals, expect_keys, expect_vals, cudf::make_mean_aggregation<groupby_aggregation>());
}

}  // namespace test
}  // namespace cudf
