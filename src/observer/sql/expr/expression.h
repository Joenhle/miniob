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
// Created by Wangyunlai on 2022/07/05.
//

#pragma once

#include <string.h>
#include <vector>
#include "storage/common/field.h"
#include "sql/expr/tuple_cell.h"

using namespace std;

class Tuple;
class TupleCellSpec;

enum class ExprType {
  NONE,
  FIELD,
  VALUE,
  VAR,
  CALCULATE,
};

enum class ExprCellType {
  NONE,
  FIELD,
  INT,
  FLOAT,
  DATE,
  AGGREGATE,
  PLUS,
  MINUS,
  MULTI,
  DIV
};

class Expression {
public:
  Expression() = default;
  virtual ~Expression() = default;

  virtual RC get_value(const Tuple &tuple, TupleCell &cell) const = 0;
  virtual std::string get_name() const = 0;
  virtual ExprType type() const = 0;
};

class FieldExpr : public Expression {
public:
  FieldExpr() = default;
  FieldExpr(const Table *table, const FieldMeta *field) : field_(table, field)
  {}

  virtual ~FieldExpr() = default;

  ExprType type() const override
  {
    return ExprType::FIELD;
  }

  Field &field()
  {
    return field_;
  }

  const Field &field() const
  {
    return field_;
  }

  const char *table_name() const
  {
    return field_.table_name();
  }

  std::string get_name() const override
  {
    return field_.field_name();
  }
  const char *field_name() const
  {
    return field_.field_name();
  }

  RC get_value(const Tuple &tuple, TupleCell &cell) const override;

private:
  Field field_;
};

class ValueExpr : public Expression {
  friend TupleCellSpec;
  friend TupleCell;

public:
  ValueExpr() = default;
  ValueExpr(const Value &value) : tuple_cell_(value.type, (char *)value.data)
  {
    if (value.type == CHARS) {
      tuple_cell_.set_length(strlen((const char *)value.data));
    }
  }

  virtual ~ValueExpr() = default;
  std::string get_name() const override;
  RC get_value(const Tuple &tuple, TupleCell &cell) const override;
  ExprType type() const override
  {
    return ExprType::VALUE;
  }

  virtual void get_tuple_cell(TupleCell &cell) const
  {
    cell = tuple_cell_;
  }

private:
  TupleCell tuple_cell_;
};

class VarExpr : public Expression {
public:
  VarExpr(std::string name, AttrType type): name_(name)
  {}

  ExprType type() const override
  {
    return ExprType::VAR;
  }
  ~VarExpr() override = default;
  RC get_value(const Tuple &tuple, TupleCell &cell) const override;
  std::string get_name() const override;

private:
  std::string name_;
};

class CalculateExpr : public Expression {
public:
  CalculateExpr(string name, vector<string> expr_cells, AttrType type) : name_(name), expr_cells_(expr_cells) {};

  ExprType type() const override { return ExprType::CALCULATE; }
  ~CalculateExpr() override = default;
  RC get_value(const Tuple &tuple, TupleCell &cell) const override;
  std::string get_name() const override;
  vector<string> expr_cells() { return expr_cells_; }
  ExprCellType get_expr_cell_type(string expr_cell) const;

private:
  string name_;
  vector<string> expr_cells_;
};
