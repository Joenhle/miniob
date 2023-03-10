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
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/insert_stmt.h"
#include "common/log/log.h"
#include "storage/common/db.h"
#include "storage/common/table.h"
#include "sql/stmt/typecaster.h"

InsertStmt::InsertStmt(Table *table, const Value *values, int value_amount, int tuple_size)
    : table_(table), values_(values), value_amount_(value_amount), tuple_size_(tuple_size) {}

RC InsertStmt::create(Db *db, const Inserts &inserts, Stmt *&stmt) {
  const char *table_name = inserts.relation_name;
  if (nullptr == db || nullptr == table_name || inserts.value_num <= 0 || inserts.tuple_num <= 0) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, value_num=%d, tuple_num=%d",
        db, table_name, inserts.value_num, inserts.tuple_num);
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  //check if the multiple inserts has the same values
  int size = inserts.tuple_size[0];
  for (int i = 1; i < inserts.tuple_num; ++i) {
    if (inserts.tuple_size[i] != size) {
      LOG_WARN("multiple inserts has the different size tuple_list");
      return RC::INVALID_ARGUMENT;
    }
  }

  // check the fields number
  Value *values = (Value *)inserts.values;
  const int value_num = inserts.value_num;
  const TableMeta &table_meta = table->table_meta();
  const int field_num = table_meta.field_num() - table_meta.sys_field_num();
  if (field_num != size) {
    LOG_WARN("schema mismatch. tuple_size=%d, field num in schema=%d", size, field_num);
    return RC::SCHEMA_FIELD_MISSING;
  }

  // check fields type
  const int sys_field_num = table_meta.sys_field_num();
  for (int i = 0; i < value_num; i++) {
    const FieldMeta *field_meta = table_meta.field((i%size) + sys_field_num);
    const AttrType field_type = field_meta->type();
    const AttrType value_type = values[i].type;
    //null value
    if (value_type == UNDEFINED) {
      if (!table_meta.is_field_nullable(i%size)) {
        LOG_WARN("the field is not nullable, but the value is null");
        return INTERNAL;
      }
      continue;
    }

    RC rc = Typecaster::attr_cast(values+i, field_type);
    if (RC::TYPECAST == rc){
      LOG_WARN("field type mismatch. table=%s, field=%s, field type=%d, value_type=%d",
          table_name, field_meta->name(), field_type, value_type);
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
    // if (field_type != value_type) { // TODO try to convert the value type to field type
    //   LOG_WARN("field type mismatch. table=%s, field=%s, field type=%d, value_type=%d",
    //       table_name, field_meta->name(), field_type, value_type);
    //   return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    // }
  }

  // everything alright
  stmt = new InsertStmt(table, values, value_num, size);
  return RC::SUCCESS;
}
