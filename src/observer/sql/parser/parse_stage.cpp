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

std::string handle_sub_query(std::string sql) {
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
        return sub_query.substr(1, sub_query.length()-2);
      }
    }
  }
  return "";
}



RC ParseStage::handle_request(StageEvent *event, bool sub_query)
{
  // 先解析
  SQLStageEvent *sql_event = static_cast<SQLStageEvent *>(event);
  const std::string &sql = sql_event->sql();
  Query *query_result = query_create();
  if (nullptr == query_result) {
    LOG_ERROR("Failed to create query.");
    return RC::INTERNAL;
  }
  RC ret = parse(sql.c_str(), query_result);
  query_result->sql = sql.c_str();
  if (ret != RC::SUCCESS) {
    // set error information to event
    LOG_ERROR("Failed to parse sql\n");
    sql_event->session_event()->set_response("FAILURE\n");
    query_destroy(query_result);
    return RC::INTERNAL;
  }
  sql_event->set_query(query_result);
  return RC::SUCCESS;
}




void ParseStage::handle_event(StageEvent *event)
{
  auto handle = [&](SQLStageEvent *sql_event, bool sub_query) {
    RC rc = handle_request(sql_event);
    if (RC::SUCCESS != rc) {
      callback_event(sql_event, nullptr);
      return rc;
    }
    if (!sub_query) {
      CompletionCallback *cb = new (std::nothrow) CompletionCallback(this, nullptr);
      if (cb == nullptr) {
        LOG_ERROR("Failed to new callback for SQLStageEvent");
        callback_event(sql_event, nullptr);
        return INTERNAL;
      }
      sql_event->push_callback(cb);
      resolve_stage_->handle_event(event);
      sql_event->done_immediate();
    } else {
      resolve_stage_->handle_event(event);
    }
    return SUCCESS;
  };

  RC rc = SUCCESS;
  LOG_TRACE("Enter\n");
  auto sql_event = static_cast<SQLStageEvent *>(event);
  std::string old_str = sql_event->sql();
  std::string temp = handle_sub_query(sql_event->sql());
  //todo 先假设只有update-select才会走子查询，且只考虑了子查询select只有一个的情况
  while (temp != "") {
    sql_event->set_sql(string(temp + ";").c_str());

    //先执行子查询
    sql_event->set_is_sub_query(true);
    if((rc = handle(sql_event, true)) != SUCCESS) {
      return;
    }
    auto sub_select_tuples = sql_event->sub_query_res();
    //子查询的结果应该只有一个值
    if (sub_select_tuples.size() == 0 || sub_select_tuples.size() > 1 || sub_select_tuples[0]->cell_num() != 1) {
      sql_event->session_event()->set_response("FAILURE\n");
      callback_event(sql_event, nullptr);
      return;
    }
    TupleCell cell;
    sub_select_tuples[0]->cell_at(0, cell);
    string new_str = old_str;
    string value = cell.to_string();
    if (cell.attr_type() == CHARS || cell.attr_type() == DATES) {
      value = "'" + value + "'";
    }
    string k;
    for (auto ch : temp) {
      if (ch == '(' || ch == ')') k += "\\";
      k += ch;
    }
    str_replace_by_regex(new_str, "\\([ ]*" + k + "[ ]*\\)", value);
    old_str = new_str;
    sql_event->set_sql(new_str.c_str());
    temp = handle_sub_query(sql_event->sql());
  }
  sql_event->set_is_sub_query(false);
  handle(sql_event, false);
  LOG_TRACE("Exit\n");
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


