/*
 * Copyright (c) 2019, NVIDIA CORPORATION.
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

#pragma once

#include <cudf/utilities/legacy/type_dispatcher.hpp>

#include <cudf/cudf.h>
#include <cudf/types.hpp>

#include <rmm/thrust_rmm_allocator.h>

namespace cudf {

namespace detail {

struct groupby {
  using index_vector = rmm::device_vector<gdf_size_type>;

  groupby(cudf::table const& key_table, bool include_nulls = false)
  : _key_table(key_table)
  , _num_keys(key_table.num_rows())
  , _include_nulls(include_nulls)
  {
    _key_sorted_order = allocate_column(gdf_dtype_of<gdf_index_type>(),
                                        key_table.num_rows(),
                                        false);

    set_key_sort_order();
    set_group_ids();
    set_group_labels();
    set_unsorted_labels();
  };

  ~groupby() {
    gdf_column_free(&_key_sorted_order);
    gdf_column_free(&_unsorted_labels);
  }

  /**
   * @brief Returns a grouped and sorted values column and a count of valid
   * values within each group
   * 
   * Sorts and groups the @p val_col where the groups are dictated by key table
   * and the elements are sorted ascending within the groups. Calculates the
   * number of valid values within each group and also returns this
   * 
   * @param val_col The value column to group and sort
   * @return std::pair<gdf_column, rmm::device_vector<gdf_size_type> > 
   *  The sorted and grouped column, and per group valid count
   */
  std::pair<gdf_column, rmm::device_vector<gdf_size_type> >
  sort_values(gdf_column const& val_col);

  /**
   * @brief Returns a table of sorted unique keys
   * 
   * The result contains a new table where each row is a unique row in the
   * sorted key table
   */
  cudf::table unique_keys();

  /**
   * @brief Returns the number of groups in the key table
   * 
   */
  gdf_size_type num_groups() { return _group_ids.size(); }

  /**
   * @brief Returns a device vector of group indices
   * 
   */
  index_vector& group_indices() { return _group_ids; }

 private:
  /**
   * @brief Set the member _key_sorted_order.
   * 
   * This member contains the sort order indices for _key_table. Gathering the
   * _key_table by _key_sorted_order would produce the sorted key table
   */
  void set_key_sort_order();

  /**
   * @brief Set the member _group_ids.
   * 
   * _group_ids contains the indices for the starting points of each group in
   * the sorted key table
   */
  void set_group_ids();

  /**
   * @brief Set the member _group_labels
   * 
   * _group_labels contains a value for each row in the sorted key column
   * signifying which group in _group_ids it belongs to
   */
  void set_group_labels();

  /**
   * @brief Set the member _unsorted_labels
   * 
   * _unsorted_labels contains the group labels but in nthe order of the 
   * unsorted _key_table so that for each row in _key_table, _unsorted_labels
   * contains the group it would belong to, after sorting
   */
  void set_unsorted_labels();

 private:

  gdf_column         _key_sorted_order;
  gdf_column         _unsorted_labels;
  cudf::table const& _key_table;

  index_vector       _group_ids;
  index_vector       _group_labels;

  gdf_size_type      _num_keys;
  bool               _include_nulls;

};

} // namespace detail
  
} // namespace cudf
