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
// Created by Meiyi & wangyunlai.wyl on 2021/5/19.
//

#ifndef __OBSERVER_STORAGE_COMMON_BPLUS_TREE_INDEX_H_
#define __OBSERVER_STORAGE_COMMON_BPLUS_TREE_INDEX_H_

#include "storage/index/index.h"
#include "storage/index/bplus_tree.h"

class BplusTreeIndex : public Index {
public:
  BplusTreeIndex() = default;
  virtual ~BplusTreeIndex() noexcept;

  RC create(const char *file_name, const IndexMeta &index_meta, const std::vector<const FieldMeta *> &field_metas);
  RC open(const char *file_name, const IndexMeta &index_meta, const std::vector<const FieldMeta *> &field_meta);
  RC close();

  RC insert_entry(const char *record, const RID *rid) override;
  RC delete_entry(const char *record, const RID *rid) override;
  RC update_entry(const char *old_record, const char *new_record, const RID *rid) override;
  bool entry_exist(const char *old_record) override;

  /**
   * 扫描指定范围的数据
   */
  IndexScanner *create_scanner(const std::vector<const char *> &left_keys, const std::vector<int> &left_lens,
      bool left_inclusive, const std::vector<const char *> &right_key, const std::vector<int> &right_lens,
      bool right_inclusive) override;

  RC sync() override;

private:
  std::vector<const char *> extract_key(const char *record);
  bool inited_ = false;
  BplusTreeHandler index_handler_;
};

class BplusTreeIndexScanner : public IndexScanner {
public:
  BplusTreeIndexScanner(BplusTreeHandler &tree_handle);
  ~BplusTreeIndexScanner() noexcept override;

  RC next_entry(RID *rid) override;
  RC destroy() override;

  RC open(const std::vector<const char *> &left_key, const std::vector<int> left_lens, bool left_inclusive,
      const std::vector<const char *> &right_key, const std::vector<int> right_len, bool right_inclusive);

private:
  BplusTreeScanner tree_scanner_;
};

#endif  //__OBSERVER_STORAGE_COMMON_BPLUS_TREE_INDEX_H_
