/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/4.
//

#pragma once

#include <stddef.h>
#include <vector>
#include <limits>
#include <sstream>
#include <bitset>

#include "rc.h"
#include "defs.h"
#include "storage/common/index_meta.h"
#include "storage/common/field_meta.h"

#define NULL_BITMAP_OFFSET 4
#define NULL_BITMAP_SIZE 4 * 8// 4 bytes
#define RECORD_NULL_RAW_BITMAP(record) (*(int *)(record+NULL_BITMAP_OFFSET))
#define RECORD_NULL_BITMAP(record) (std::bitset<NULL_BITMAP_SIZE>(*(int *)(record+NULL_BITMAP_OFFSET)))

class Field;

struct RID {
  PageNum page_num;  // record's page number
  SlotNum slot_num;  // record's slot number
  // bool    valid;    // true means a valid record

  RID() = default;
  RID(const PageNum _page_num, const SlotNum _slot_num)
    : page_num(_page_num), slot_num(_slot_num)
  {}

  const std::string to_string() const
  {
    std::stringstream ss;

    ss << "PageNum:" << page_num << ", SlotNum:" << slot_num;

    return ss.str();
  }

  bool operator==(const RID &other) const
  {
    return page_num == other.page_num && slot_num == other.slot_num;
  }

  bool operator!=(const RID &other) const
  {
    return !(*this == other);
  }

  static int compare(const RID *rid1, const RID *rid2)
  {
    int page_diff = rid1->page_num - rid2->page_num;
    if (page_diff != 0) {
      return page_diff;
    } else {
      return rid1->slot_num - rid2->slot_num;
    }
  }

  /**
   * 返回一个不可能出现的最小的RID
   * 虽然page num 0和slot num 0都是合法的，但是page num 0通常用于存放meta数据，所以对数据部分来说都是
   * 不合法的. 这里在bplus tree中查找时会用到。
   */
  static RID *min()
  {
    static RID rid{0, 0};
    return &rid;
  }
  static RID *max()
  {
    static RID rid{std::numeric_limits<PageNum>::max(), std::numeric_limits<SlotNum>::max()};
    return &rid;
  }
};

class Record
{
public:
  Record() = default;
  ~Record() = default;
  using NullBitMap = std::bitset<NULL_BITMAP_SIZE>;

  void set_data(char *data) {
    this->data_ = data;
    null_bitmap_.reset();
    null_bitmap_ |= RECORD_NULL_RAW_BITMAP(data);
  }
  char *data() { return this->data_; }
  const char *data() const { return this->data_; }
  void set_rid(const RID &rid) { this->rid_ = rid; }
  void set_rid(const PageNum page_num, const SlotNum slot_num) { this->rid_.page_num = page_num; this->rid_.slot_num = slot_num; }
  RID & rid() { return rid_; }
  const RID &rid() const { return rid_; };
  const NullBitMap& get_null_bitmap() const{
    return null_bitmap_;
  }

  std::vector<int> null_fields() {
    std::vector<int> arr;
    for (int i = 0; i < NULL_BITMAP_SIZE; ++i) {
      if(null_bitmap_.test(i)){
        arr.push_back(i);
      }
    }
    return arr;
  };

private:
  RID                            rid_;

  // the data buffer
  // record will not release the memory
  char *                         data_ = nullptr;
  NullBitMap null_bitmap_;
};
