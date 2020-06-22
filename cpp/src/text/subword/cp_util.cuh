/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
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

#include <text/subword/cp_data.h>

__device__ __forceinline__ uint32_t get_cp_metadata(uint32_t* meta_data_ptr, uint32_t cp)
{
  return meta_data_ptr[cp];
}

__device__ __forceinline__ uint32_t get_first_cp(uint32_t cp_metadata)
{
  return cp_metadata & NEW_CP_MASK;
}

__device__ __forceinline__ uint32_t extract_token_cat(uint32_t cp_metadata)
{
  return (cp_metadata >> TOKEN_CAT_SHIFT) & TOKEN_CAT_MASK;
}

__device__ __forceinline__ uint32_t should_remove_cp(uint32_t cp_metadata, bool lower_case)
{
  int cat = extract_token_cat(cp_metadata);
  return (cat == TOKEN_CAT_REMOVE_CHAR) || (lower_case && (cat == TOKEN_CAT_REMOVE_CHAR_IF_LOWER));
}

__device__ __forceinline__ uint32_t should_add_spaces(uint32_t cp_metadata, bool lower_case)
{
  int cat = extract_token_cat(cp_metadata);
  return (cat == TOKEN_CAT_ADD_SPACE) || (lower_case && (cat == TOKEN_CAT_ADD_SPACE_IF_LOWER));
}

__device__ __forceinline__ uint32_t always_replace(uint32_t cp_metadata)
{
  return extract_token_cat(cp_metadata) == TOKEN_CAT_ALWAYS_REPLACE;
}

__device__ __forceinline__ uint32_t is_multi_char_transform(uint32_t cp_metadata)
{
  return (cp_metadata >> MULTICHAR_SHIFT) & MULTICHAR_MASK;
}

__device__ __forceinline__ uint64_t get_extra_cps(uint64_t* aux_table_ptr, uint32_t cp)
{
  return aux_table_ptr[cp];
}