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

#include "storage/index/bplus_tree_index.h"
#include "common/log/log.h"

BplusTreeIndex::~BplusTreeIndex() noexcept
{
  close();
}

RC BplusTreeIndex::create(
    const char *file_name, const IndexMeta &index_meta, const std::vector<const FieldMeta *> &field_metas)
{
  if (inited_) {
    LOG_WARN("Failed to create index due to the index has been created before. file_name:%s, index:%s, field:%s",
        file_name,
        index_meta.name(),
        index_meta.field().c_str());
    return RC::RECORD_OPENNED;
  }

  Index::init(index_meta, field_metas);

  RC rc = index_handler_.create(file_name, field_metas, index_meta.is_unique());
  if (RC::SUCCESS != rc) {
    LOG_WARN("Failed to create index_handler, file_name:%s, index:%s, field:%s, rc:%s",
        file_name,
        index_meta.name(),
        index_meta.field().c_str(),
        strrc(rc));
    return rc;
  }

  inited_ = true;
  LOG_INFO(
      "Successfully create index, file_name:%s, index:%s, field:%s", file_name, index_meta.name(), index_meta.field().c_str());
  return RC::SUCCESS;
}

RC BplusTreeIndex::open(
    const char *file_name, const IndexMeta &index_meta, const std::vector<const FieldMeta *> &field_meta)
{
  if (inited_) {
    LOG_WARN("Failed to open index due to the index has been initedd before. file_name:%s, index:%s, field:%s",
        file_name,
        index_meta.name(),
        index_meta.field().c_str());
    return RC::RECORD_OPENNED;
  }

  Index::init(index_meta, field_meta);

  RC rc = index_handler_.open(file_name);
  if (RC::SUCCESS != rc) {
    LOG_WARN("Failed to open index_handler, file_name:%s, index:%s, field:%s, rc:%s",
        file_name,
        index_meta.name(),
        index_meta.field().c_str(),
        strrc(rc));
    return rc;
  }

  inited_ = true;
  LOG_INFO(
      "Successfully open index, file_name:%s, index:%s, field:%s", file_name, index_meta.name(), index_meta.field().c_str());
  return RC::SUCCESS;
}

RC BplusTreeIndex::close()
{
  if (inited_) {
    LOG_INFO("Begin to close index, index:%s, field:%s", index_meta_.name(), index_meta_.field().c_str());
    index_handler_.close();
    inited_ = false;
  }
  LOG_INFO("Successfully close index.");
  return RC::SUCCESS;
}

RC BplusTreeIndex::insert_entry(const char *record, const RID *rid)
{
  auto key = extract_key(record);
  return index_handler_.insert_entry(key, rid);
}

RC BplusTreeIndex::delete_entry(const char *record, const RID *rid)
{
  auto key = extract_key(record);
  return index_handler_.delete_entry(key, rid);
}

IndexScanner *BplusTreeIndex::create_scanner(const std::vector<const char *> &left_keys,
    const std::vector<int> &left_lens, bool left_inclusive, const std::vector<const char *> &right_key,
    const std::vector<int> &right_lens, bool right_inclusive)
{
  BplusTreeIndexScanner *index_scanner = new BplusTreeIndexScanner(index_handler_);
  RC rc = index_scanner->open(left_keys, left_lens, left_inclusive, right_key, right_lens, right_inclusive);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open index scanner. rc=%d:%s", rc, strrc(rc));
    delete index_scanner;
    return nullptr;
  }
  return index_scanner;
}

RC BplusTreeIndex::sync()
{
  return index_handler_.sync();
}
std::vector<const char *> BplusTreeIndex::extract_key(const char *record)
{
  std::vector<const char *> key;
  for (const auto &field : field_meta_) {
    key.push_back(record + field->offset());
  }
  return key;
}
RC BplusTreeIndex::update_entry(
    const char *old_record, const char *new_record, const RID *rid, bool need_to_del_old, bool need_to_ins_new)
{
  const auto old_key = extract_key(old_record);
  const auto new_key = extract_key(new_record);
  return index_handler_.update_entry(old_key, new_key, rid, need_to_del_old, need_to_ins_new);
}
bool BplusTreeIndex::entry_exist(const char *record)
{
  const auto key = extract_key(record);
  return index_handler_.entry_exists(key);
}

////////////////////////////////////////////////////////////////////////////////
BplusTreeIndexScanner::BplusTreeIndexScanner(BplusTreeHandler &tree_handler) : tree_scanner_(tree_handler)
{}

BplusTreeIndexScanner::~BplusTreeIndexScanner() noexcept
{
  tree_scanner_.close();
}

RC BplusTreeIndexScanner::open(const std::vector<const char *> &left_key, const std::vector<int> left_lens,
    bool left_inclusive, const std::vector<const char *> &right_key, const std::vector<int> right_len,
    bool right_inclusive)
{
  return tree_scanner_.open(left_key, left_lens, left_inclusive, right_key, right_len, right_inclusive);
}

RC BplusTreeIndexScanner::next_entry(RID *rid)
{
  return tree_scanner_.next_entry(rid);
}

RC BplusTreeIndexScanner::destroy()
{
  delete this;
  return RC::SUCCESS;
}
