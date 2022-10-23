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
#include "storage/common/field.h"
#include "sql/expr/tuple_cell.h"

class Tuple;
class TupleCellSpec;

enum class ExprType {
  NONE,
  FIELD,
  VALUE,
  VAR,
  CALCULATE,
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

  std::string get_name() const
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

  ExprType type() const
  {
    return ExprType::VAR;
  }
  ~VarExpr() override = default;
  RC get_value(const Tuple &tuple, TupleCell &cell) const override;
  std::string get_name() const override;

private:
  std::string name_;
};

// class TreeExpr : public Expression {
// public:
//   ~TreeExpr() override = default;
//   RC get_value(const Tuple &tuple, TupleCell &cell) const override;
//   ExprType type() const override;
// };
//  class CastedExpr: public FieldExpr, public ValueExpr{
//  public:
//    CastedExpr(Expression *expr):expr_origin_(expr){}
//    virtual ~CastedExpr() = default;
//    RC get_value(const Tuple &tuple, TupleCell &cell) const override
//    {
//      RC rc = expr_origin_->get_value(tuple, cell);
//      if(rc != RC::SUCCESS){
//        return rc;
//      }
//      Value attr_value;
//    }
//
//  private:
//    AttrType old_type_;
//    AttrType new_type_;
//    Expression *expr_origin_;
//  };