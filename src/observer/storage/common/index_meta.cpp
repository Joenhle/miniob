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
// Created by Meiyi & Wangyunlai.wyl on 2021/5/18.
//

#include "storage/common/index_meta.h"
#include "storage/common/field_meta.h"
#include "storage/common/table_meta.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "util/util.h"
#include "rc.h"
#include "json/json.h"

const static Json::StaticString FIELD_NUM_OF_FIELDS("num_of_field");
const static Json::StaticString FIELD_INDEX_NAME("name");
const static Json::StaticString FIELD_FIELD_NAMES("field_names");
const static Json::StaticString FIELD_IS_UNIQUE("is_unique");

RC IndexMeta::init(const char *name, const FieldMeta &field)
{
  if (common::is_blank(name)) {
    LOG_ERROR("Failed to init index, name is empty.");
    return RC::INVALID_ARGUMENT;
  }

  name_ = name;
  fields_ = {field.name()};
  num_of_fields_ = 1;
  return RC::SUCCESS;
}

RC IndexMeta::init(const char *name, const std::vector<const FieldMeta *> &field, bool is_unique)
{
  if (common::is_blank(name)) {
    LOG_ERROR("Failed to init index, name is empty.");
    return RC::INVALID_ARGUMENT;
  }

  name_ = name;
  for (const auto &field : field) {
    fields_.push_back(field->name());
  }
  num_of_fields_ = field.size();
  is_unique_ = is_unique;
  return RC::SUCCESS;
}

void IndexMeta::to_json(Json::Value &json_value) const
{
  Json::Value fields;
  for (const auto &field : fields_) {
    fields.append(field);
  }
  json_value[FIELD_NUM_OF_FIELDS] = num_of_fields_;
  json_value[FIELD_INDEX_NAME] = name_;
  json_value[FIELD_FIELD_NAMES] = fields;
  json_value[FIELD_IS_UNIQUE] = is_unique_;
}

RC IndexMeta::from_json(const TableMeta &table, const Json::Value &json_value, IndexMeta &index)
{
  const Json::Value &num_of_fields_value = json_value[FIELD_NUM_OF_FIELDS];
  const Json::Value &name_value = json_value[FIELD_INDEX_NAME];
  const Json::Value &field_values = json_value[FIELD_FIELD_NAMES];
  const Json::Value &is_unique = json_value[FIELD_IS_UNIQUE];
  if (!num_of_fields_value.isInt()) {
    LOG_ERROR("Index num of fields is not an integer. json value=%s", name_value.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }
  const int num_of_fields = num_of_fields_value.asInt();
  if (num_of_fields_value <= 0) {
    LOG_ERROR("Index num of fields <= 0. json value=%s", name_value.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }
  if (!name_value.isString()) {
    LOG_ERROR("Index name is not a string. json value=%s", name_value.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }
  if (!field_values.isArray()) {
    LOG_ERROR("Fields of index [%s] is not an Array. json value=%s",
        name_value.asCString(),
        field_values.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }
  if (field_values.size() != num_of_fields) {
    LOG_ERROR("Num of fields of index [%s] doesn't match. json value=%s",
        name_value.asCString(),
        field_values.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }
  if (!is_unique.isBool()) {
    LOG_ERROR("Field of is_unique of index [%s] is not bool. json value=%s",
              name_value.asCString(),
              is_unique.toStyledString().c_str());
    return RC::GENERIC_ERROR;
  }
  std::vector<const FieldMeta*> fields(num_of_fields);
  for (int i = 0; i < num_of_fields; i++) {
    auto field = table.field(field_values[i].asCString());
    if (nullptr == field) {
      LOG_ERROR("Deserialize index [%s]: no such field: %s", name_value.asCString(), field_values.asCString());
      return RC::SCHEMA_FIELD_MISSING;
    }
    fields[i] = field;
  }

  return index.init(name_value.asCString(), fields, false);
}

const char *IndexMeta::name() const
{
  return name_.c_str();
}
const char *IndexMeta::field(int idx) const
{
  return fields_[idx].c_str();
}
std::string IndexMeta::field() const
{
  return vector2str(fields_);
}

void IndexMeta::desc(std::ostream &os) const
{
  if (fields_.size() == 1) {
    os << "index name=" << name_ << ", field=" << fields_[0];
  } else {
    os << "index name=" << name_ << ", field=(";
    for (size_t i = 0, sz = fields_.size(); i < sz; i++) {
      os << fields_[i] << (i == sz - 1 ? ")" : ", ");
    }
  }
}
RC IndexMeta::get_field_metas(TableMeta& table_meta, std::vector<const FieldMeta*>&out) const
{
  std::vector<FieldMeta> field_metas;
  for (const auto &field_name : fields_){
    auto field_meta = table_meta.field(field_name.c_str());
    if(field_meta == nullptr){
      LOG_ERROR("Found invalid index meta info which has a non-exists field. table=%s, index=%s, field=%s",
                table_meta.name(),
                name(),
                field_name.c_str());
      out.clear();
      return RC::SCHEMA_FIELD_NOT_EXIST;
    }
    out.push_back(field_meta);
  }
  return RC::SUCCESS;
}