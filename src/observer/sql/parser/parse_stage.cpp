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
// Created by Longda on 2021/4/13.
//

#include <string.h>
#include <string>
#include <regex>
#include "parse_stage.h"
#include "util/util.h"
#include "common/conf/ini.h"
#include "common/io/io.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/seda/timer_stage.h"
#include "event/session_event.h"
#include "event/sql_event.h"
#include "sql/parser/parse.h"
#include <utility>
#include <unordered_set>
using namespace std;

using namespace common;

//! Constructor
ParseStage::ParseStage(const char *tag) : Stage(tag)
{}

//! Destructor
ParseStage::~ParseStage()
{}

//! Parse properties, instantiate a stage object
Stage *ParseStage::make_stage(const std::string &tag)
{
  ParseStage *stage = new (std::nothrow) ParseStage(tag.c_str());
  if (stage == nullptr) {
    LOG_ERROR("new ParseStage failed");
    return nullptr;
  }
  stage->set_properties();
  return stage;
}

//! Set properties for this object set in stage specific properties
bool ParseStage::set_properties()
{
  //  std::string stageNameStr(stageName);
  //  std::map<std::string, std::string> section = theGlobalProperties()->get(
  //    stageNameStr);
  //
  //  std::map<std::string, std::string>::iterator it;
  //
  //  std::string key;

  return true;
}

//! Initialize stage params and validate outputs
bool ParseStage::initialize()
{
  LOG_TRACE("Enter");

  std::list<Stage *>::iterator stgp = next_stage_list_.begin();
  // optimize_stage_ = *(stgp++);
  resolve_stage_ = *(stgp++);

  LOG_TRACE("Exit");
  return true;
}

//! Cleanup after disconnection
void ParseStage::cleanup()
{
  LOG_TRACE("Enter");

  LOG_TRACE("Exit");
}

pair<string, SubQueryOper> handle_sub_query(std::string sql) {
  std::smatch result;
  std::regex pattern("\\([ ]*[Ss][Ee][Ll][Ee][Cc][Tt] ");
  std::string::const_iterator iterStart = sql.begin(), iterEnd = sql.end();
  if (std::regex_search(iterStart, iterEnd, result, pattern)) {
    std::string sub_query;
    auto index = result[0].first;
    int num = 0;
    while (index < sql.end()) {
      if (*index == '(') num++;
      else if (*index == ')') num--;
      sub_query += *index;
      index++;
      if (num == 0) {
        sub_query = sub_query.substr(1, sub_query.length()-2);
        break;
      }
    }
    string oper;
    index = result[0].first-1;
    if (*index != ' ') {
      while (index >= sql.begin() && *index != ' ') {
        oper = *index + oper;
        index--;
      }
    } else {
      num = 0;
      while (index >= sql.begin()) {
        if (*index == ' ') num++;
        else {
          if (num > 0) num *= -1;
          oper = *index + oper;
        }
        if (num == 0) {
          break;
        }
        index--;
      }
    }
    if (oper == "in" || oper == "IN") return {sub_query, SubQueryOper::IN};
    if (oper == "exists" || oper == "EXISTS") return {sub_query, SubQueryOper::EXIST};
    return {sub_query, SubQueryOper::COMPARE};
  }
  return pair<string, SubQueryOper>{"", SubQueryOper::NO_OPER};
}

RC ParseStage::handle_request(StageEvent *event, bool sub_query)
{
  // 先解析
  SQLStageEvent *sql_event = static_cast<SQLStageEvent *>(event);
  sql_event->set_is_sub_query(sub_query);
  const std::string &sql = sql_event->sql();
  Query *query_result = query_create();
  if (nullptr == query_result) {
    LOG_ERROR("Failed to create query.");
    callback_event(sql_event, nullptr);
    return RC::INTERNAL;
  }
  RC ret = parse(sql.c_str(), query_result);
  query_result->sql = sql.c_str();
  if (ret != RC::SUCCESS) {
    // set error information to event
    LOG_ERROR("Failed to parse sql\n");
    sql_event->session_event()->set_response("FAILURE\n");
    query_destroy(query_result);
    callback_event(sql_event, nullptr);
    return RC::INTERNAL;
  }
  sql_event->set_query(query_result);
  resolve_stage_->handle_event(sql_event);
  return RC::SUCCESS;
}

void ParseStage::handle_event(StageEvent *event)
{
  RC rc = SUCCESS;
  LOG_TRACE("Enter\n");
  auto sql_event = static_cast<SQLStageEvent *>(event);
  CompletionCallback *cb = new (std::nothrow) CompletionCallback(this, nullptr);
  if (cb == nullptr) {
    LOG_ERROR("Failed to new callback for SQLStageEvent");
    callback_event(sql_event, nullptr);
    return;
  }
  sql_event->push_callback(cb);
  std::string old_str = sql_event->sql();
  auto sub_query_pair =  handle_sub_query(sql_event->sql());
  auto rebuild = [](string temp){
    string k;
    for (auto ch : temp) {
      if (ch == '(' || ch == ')' || ch == '*') k += "\\";
      k += ch;
    }
    return k;
  };
  string new_sql = sql_event->sql();
  while (sub_query_pair.second != SubQueryOper::NO_OPER) {
    auto sub_query_str = sub_query_pair.first;
    if (sub_query_pair.second == SubQueryOper::IN || sub_query_pair.second == SubQueryOper::COMPARE) {
      sql_event->set_sql(string(sub_query_str + ";").c_str());
      if((rc = handle_request(sql_event, true)) != SUCCESS) {
        return;
      }
      auto sub_select_tuples = sql_event->sub_query_res();
      if ((sub_query_pair.second == SubQueryOper::IN && (sub_select_tuples.size() == 0 || sub_select_tuples[0]->cell_num() != 1)) ||
          (sub_query_pair.second == SubQueryOper::COMPARE && (sub_select_tuples.size() != 1 || sub_select_tuples[0]->cell_num() != 1))) {
        sql_event->session_event()->set_response("FAILURE\n");
        callback_event(sql_event, nullptr);
        return;
      }
      TupleCell cell;
      string value;
      for (int i = 0; i < sub_select_tuples.size(); ++i) {
        sub_select_tuples[i]->cell_at(0, cell);
        if (i == sub_select_tuples.size() - 1) {
          value += cell.to_string();
        } else {
          value += cell.to_string() + ",";
        }
      }
      sub_query_str = rebuild(sub_query_str);
      if (sub_query_pair.second == SubQueryOper::COMPARE) {
        str_replace_by_regex(new_sql, "\\([ ]*" + sub_query_str + "[ ]*\\)", value);
      } else {
        str_replace_by_regex(new_sql, sub_query_str, value);
      }
      sql_event->set_sql(new_sql.c_str());
      sub_query_pair =  handle_sub_query(sql_event->sql());
    } else if (sub_query_pair.second == SubQueryOper::EXIST) {
      auto query_temp = query_create();
      parse(string(sub_query_str + ";").c_str(), query_temp);
      string relation_name = query_temp->sstr.selection.relations[0].name;
      vector<string> tables = {relation_name};
      vector<string> fields;
      for (int i = 0; i < query_temp->sstr.selection.condition_num; ++i) {
        auto condition = query_temp->sstr.selection.conditions[i];
        if (condition.left_type == 2 && condition.left_attr.relation_name != nullptr && string(condition.left_attr.relation_name) != relation_name) {
          tables.push_back(string(condition.left_attr.relation_name));
          fields.push_back(string(condition.left_attr.relation_name) + "." + string(condition.left_attr.attribute_name));
        }
        if (condition.right_type == 2 && condition.right_attr.relation_name != nullptr && string(condition.right_attr.relation_name) != relation_name) {
          tables.push_back(string(condition.right_attr.relation_name));
          fields.push_back(string(condition.right_attr.relation_name) + "." + string(condition.right_attr.attribute_name));
        }
      }
      string sub_str = "select ";
      for (int i = 0; i < fields.size(); ++i) {
        if (i == fields.size() - 1) sub_str += (fields[i] + " from ");
        else sub_str += (fields[i] + ",");
      }
      for (int i = 0; i < tables.size(); ++i) {
        if (i == tables.size() - 1) sub_str += (tables[i] + " ");
        else sub_str += (tables[i] + ",");
      }
      string temp = sub_query_pair.first;
      str_replace_by_regex(temp, "[Ss][Ee][Ll][Ee][Cc][Tt].*[Ww][Hh][Ee][Rr][Ee]", "");
      sub_str += "where " + temp;
      sql_event->set_sql(string(sub_str + ";").c_str());
      if((rc = handle_request(sql_event, true)) != SUCCESS) {
        return;
      }
      auto sub_select_tuples = sql_event->sub_query_res();
      vector<string> in_values;
      for (int field = 0; field < fields.size(); ++field) {
        TupleCell cell;
        string value;
        unordered_set<string> value_set;
        for (int i = 0; i < sub_select_tuples.size(); ++i) {
          sub_select_tuples[i]->cell_at(field, cell);
          if (value_set.count(cell.to_string()) == 1) {
            continue;
          }
          value_set.insert(cell.to_string());
          if (i == sub_select_tuples.size() - 1) {
            value += cell.to_string();
          } else {
            value += cell.to_string() + ",";
          }
        }
        in_values.push_back(value);
      }
      bool is_not_exists = str_contains_by_regex(new_sql, "[Nn][Oo][Tt][ ]*[Ee][Xx][Ii][Ss][Tt][Ss]");
      string replaced_str = fields[0];
      if (is_not_exists) {
        replaced_str += (" not in (" + in_values[0] + ") ");
        str_replace_by_regex(new_sql, "[Nn][Oo][Tt][ ]*[Ee][Xx][Ii][Ss][Tt][Ss][ ]*\\([ ]*" + rebuild(sub_query_pair.first) + "[ ]*\\)", replaced_str);
      } else {
        replaced_str += (" in (" + in_values[0] + ") ");
        str_replace_by_regex(new_sql, "[Ee][Xx][Ii][Ss][Tt][Ss][ ]*\\([ ]*" + rebuild(sub_query_pair.first) + "[ ]*\\)", replaced_str);
      }
      sql_event->set_sql(new_sql.c_str());
      sub_query_pair =  handle_sub_query(sql_event->sql());
    }
  }
  LOG_TRACE("Exit\n");
  if ((rc = handle_request(sql_event, false)) != SUCCESS) {
    return;
  } else {
    sql_event->done_immediate();
  }
  return;
}



void ParseStage::callback_event(StageEvent *event, CallbackContext *context)
{
  LOG_TRACE("Enter\n");
  SQLStageEvent *sql_event = static_cast<SQLStageEvent *>(event);
  sql_event->session_event()->done_immediate();
  sql_event->done_immediate();
  LOG_TRACE("Exit\n");
  return;
}


