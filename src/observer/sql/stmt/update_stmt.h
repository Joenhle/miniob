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

#pragma once

#include "rc.h"
#include "sql/stmt/stmt.h"
#include "filter_stmt.h"
#include "select_stmt.h"
class Table;

class UpdateStmt : public Stmt
{
public:

  UpdateStmt() = default;
  StmtType type() const override{
    return StmtType::UPDATE;
  }
public:
  static RC create(Db *db, const Updates &update_sql, Stmt *&stmt);

public:
  Table *table() const {return table_;}
  std::vector<Value *>values() const { return values_; }
  int value_amount() const { return value_amount_; }
  std::vector<std::string> update_attr()const{
    return update_attr_;
  }
  FilterStmt *filter_stmt()const{
    return filter_stmt_;
  }

private:
  Table *table_ = nullptr;
  //Value *values_ = nullptr;
  int value_amount_ = 0;
  std::vector<std::string> update_attr_;
  std::vector<Value *> values_;
  FilterStmt * filter_stmt_ = nullptr;
};

