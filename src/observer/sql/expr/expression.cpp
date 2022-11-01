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

#include "sql/expr/tuple.h"
#include "util/util.h"
#include <stack>


RC FieldExpr::get_value(const Tuple &tuple, TupleCell &cell) const
{
  return tuple.find_cell(field_, cell);
}

RC ValueExpr::get_value(const Tuple &tuple, TupleCell & cell) const
{
  cell = tuple_cell_;
  return RC::SUCCESS;
}
std::string ValueExpr::get_name() const
{
  return "";
}
RC VarExpr::get_value(const Tuple &tuple, TupleCell &cell) const
{
  auto vtuple = dynamic_cast<const VTuple *> (&tuple);
  TupleCellSpec spec{(Expression *)this};
  return vtuple->find_cell(spec, cell);
}
std::string VarExpr::get_name() const
{
  return name_;
}

ExprCellType CalculateExpr::get_expr_cell_type(string cell) const
{
  auto aggregate_funcs = {"max", "MAX", "min", "MIN", "sum", "SUM", "count", "COUNT", "AVG", "avg"};
  for (auto func : aggregate_funcs) {
    if (cell.find(func) != cell.npos) {
      return ExprCellType::AGGREGATE;
    }
  }
  if (cell == "+") return ExprCellType::PLUS;
  if (cell == "-") return ExprCellType::MINUS;
  if (cell == "*") return ExprCellType::MULTI;
  if (cell == "/") return ExprCellType::DIV;
  if (str_match_by_regex(cell, "[0-9]+\\-[0-9]+\\-[0-9]+")) return ExprCellType::DATE;
  if (str_match_by_regex(cell, "[0-9]+.[0-9]+")) return ExprCellType::FLOAT;
  if (str_match_by_regex(cell, "[\\-]?[0-9]+")) return ExprCellType::INT;
  return ExprCellType::FIELD;
}

RC CalculateExpr::get_value(const Tuple &tuple, TupleCell &tuple_cell) const
{
  auto vtuple = VTuple(tuple);
  stack<float> cell_stack;
  for (auto cell : expr_cells_) {
    auto type = get_expr_cell_type(cell);
    if (type >= ExprCellType::PLUS && type <= ExprCellType::DIV) {
      if (cell_stack.size() >= 2) {
        float b = cell_stack.top();
        cell_stack.pop();
        float a = cell_stack.top();
        cell_stack.pop();
        if (type == ExprCellType::PLUS) cell_stack.push(a+b);
        if (type == ExprCellType::MINUS) cell_stack.push(a-b);
        if (type == ExprCellType::MULTI) cell_stack.push(a*b);
        if (type == ExprCellType::DIV) {
          if (b == 0) {
            return RC::DIV_ZERO;
          }
          cell_stack.push(a/b);
        }
      } else if (cell_stack.size() == 1 && type == ExprCellType::MINUS) {
        float b = cell_stack.top();
        cell_stack.pop();
        cell_stack.push(-1 * b);
      } else {
        return INVALID_ARGUMENT;
      }
    } else if (type == ExprCellType::FIELD || type == ExprCellType::AGGREGATE){
      VarExpr* varExpr = new VarExpr(cell, AttrType::UNDEFINED);
      auto spec = make_shared<TupleCellSpec>(varExpr);
      TupleCell temp;
      vtuple.find_cell(*spec, temp);
      float data = 0;
      if (temp.attr_type() == INTS) {
        data = *(int*)temp.data();
      } else if (temp.attr_type() == FLOATS) {
        data = *(float *)temp.data();
      }
      cell_stack.push(data);
    } else if (type == ExprCellType::INT || type == ExprCellType::FLOAT) {
      float data = atof(cell.c_str());
      cell_stack.push(data);
    } else {
      LOG_WARN("invalid data type in calculate expr");
      return INVALID_ARGUMENT;
    }
  }
  if (cell_stack.size() != 1) {
    return INTERNAL;
  }
  float temp = cell_stack.top();
  AttrType res_type = int(temp) == temp ? AttrType::INTS : AttrType::FLOATS;
  if (res_type == AttrType::INTS) {
    tuple_cell.set_type(AttrType::INTS);
    tuple_cell.set_length(sizeof(int));
    int* res = new int((int)temp);
    tuple_cell.set_data((char*)res);
  } else {
    tuple_cell.set_type(AttrType::FLOATS);
    tuple_cell.set_length(sizeof(float));
    float* res = new float(temp);
    tuple_cell.set_data((char*)res);
  }
  return SUCCESS;
}



std::string CalculateExpr::get_name() const
{
  return name_;
}
