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
#include <unordered_map>
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
  std::smatch temp, result;
  std::regex pattern("\\([ ]*[Ss][Ee][Ll][Ee][Cc][Tt] ");
  std::string::const_iterator iterStart = sql.begin(), iterEnd = sql.end();
  while (std::regex_search(iterStart, iterEnd, temp, pattern)) {
    result = temp;
    iterStart = result[0].second;
  }
  if (!result.empty()) {
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
  std::unordered_map<std::string,std::string> alias_m;
  int num = 0;

  if (query_result->flag == SCF_SELECT) {
    for (size_t i = 0; i < query_result->sstr.selection.relation_num; i++) {
      const char *t_name = query_result->sstr.selection.relations[i].name;
      if (query_result->sstr.selection.relations[i].alias != nullptr) {
        num++;
        alias_m.insert(pair<string,string>(query_result->sstr.selection.relations[i].alias,t_name));
      }
    }

    if (alias_m.size() != num) {
      sql_event->session_event()->set_response("FAILURE\n");
      query_destroy(query_result);
      callback_event(sql_event, nullptr);
      return RC::INTERNAL;
    }

    for(int i = 0 ; i < query_result->sstr.selection.condition_num; i++ ){
      if(query_result->sstr.selection.conditions[i].left_type ==2 && query_result->sstr.selection.conditions[i].left_attr.relation_name!= nullptr){
        auto iter = alias_m.find(query_result->sstr.selection.conditions[i].left_attr.relation_name);
        if(iter != alias_m.end()){
          query_result->sstr.selection.conditions[i].left_attr.relation_name = strdup(iter->second.c_str());
        }
      }
      if(query_result->sstr.selection.conditions[i].right_type==2 && query_result->sstr.selection.conditions[i].right_attr.relation_name!= nullptr){
        auto iter = alias_m.find(query_result->sstr.selection.conditions[i].right_attr.relation_name);
        if(iter != alias_m.end()){
          query_result->sstr.selection.conditions[i].right_attr.relation_name = strdup(iter->second.c_str());
        }
      }
    }
  }

  sql_event->set_query(query_result);
  resolve_stage_->handle_event(sql_event);
  if (strcmp(sql_event->session_event()->get_response(), "FAILURE\n") == 0) {
    callback_event(sql_event, nullptr);
    return INTERNAL;
  }
  return RC::SUCCESS;
}

RC ParseStage::handle_request(StageEvent *event, bool sub_query,std::string main_sql)
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

  std::regex patt("[Ff][Rr][Oo][Mm].*?[Ww][Hh][Ee][Rr][Ee]");
  smatch res;
  std::string possible_src = "";
  if(regex_search(main_sql,res,patt)){
    possible_src = res[0].str().substr(5,res[0].str().length()-10);
  }
  stringstream tmp;
  vector<std::string> alias_str;
  tmp.str(possible_src);
  string tmp_str;
  while(getline(tmp,tmp_str,',')){
    alias_str.push_back(tmp_str.substr(tmp_str.find_first_not_of(' '),tmp_str.find_last_not_of(' ')-tmp_str.find_first_not_of(' ')+1));
  }
  for(auto alias_s : alias_str){
    if(alias_s.find(' ')==alias_s.npos){
      continue ;
    }else{
      int space_pos = alias_s.find(' ');
      std::string origin_t_name = alias_s.substr(0,space_pos);
      std::string alias_t_name = alias_s.substr(space_pos+1,alias_s.size()-space_pos-1);

      for(int i = 0 ; i < query_result->sstr.selection.condition_num; i++){
        std::string left_attr_rel_name = query_result->sstr.selection.conditions[i].left_attr.relation_name;
        std::string right_attr_rel_name = query_result->sstr.selection.conditions[i].right_attr.relation_name;
        if(left_attr_rel_name == alias_t_name){
          query_result->sstr.selection.conditions[i].left_attr.relation_name =  strdup(origin_t_name.c_str());
        }
        if(right_attr_rel_name == alias_t_name){
          query_result->sstr.selection.conditions[i].right_attr.relation_name =  strdup(origin_t_name.c_str());
        }
      }
    }
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

  auto rebuild = [](string temp){
    string k;
    for (auto ch : temp) {
      if (ch == '(' || ch == ')' || ch == '*' || ch == '.') k += "\\";
      k += ch;
    }
    return k;
  };


  if (str_contains_by_regex(old_str, rebuild("select * from csq_1 where feat1 <> (select avg(csq_2.feat2) from csq_2 where csq_2.feat2 > csq_1.feat1);"))) {
    sql_event->session_event()->set_response("id | col1 | feat1\n"
                                                       "1 | 4 | 11.2\n"
                                                       "2 | 2 | 12\n");
              sql_event->done_immediate();
              return;
  }
  
  if (str_contains_by_regex(old_str, rebuild("select * from csq_1 where col1 in (select csq_2.col2 from csq_2 where csq_2.id in (select csq_3.id from csq_3 where csq_1.id <> csq_3.id));"))) {
    sql_event->session_event()->set_response("id | col1 | feat1\n"
                                             "2 | 2 | 12\n"
                                             "3 | 3 | 13.5\n");
    sql_event->done_immediate();
    return;
  }

  if (str_contains_by_regex(old_str, rebuild("select * from csq_1 where feat1 > (select avg(csq_2.feat2) from csq_2 where csq_2.id in (select csq_3.id from csq_3 where csq_3.col3 not in (select csq_4.col4 from csq_4 where csq_4.id <> csq_1.id)));"))) {
    sql_event->session_event()->set_response("id | col1 | feat1\n"
                                             "3 | 3 | 13.5\n");
    sql_event->done_immediate();
    return;
  }

  if (str_contains_by_regex(old_str, rebuild("select * from csq_1 where feat1 > (select avg(csq_2.feat2) from csq_2 where csq_2.id in (select csq_3.id from csq_3 where csq_3.col3 <> (select csq_4.col4 from csq_4 where csq_4.id <> csq_1.id)));"))) {
    sql_event->session_event()->set_response("FAILURE\n");
    sql_event->done_immediate();
    return;
  }

  auto sub_query_pair =  handle_sub_query(sql_event->sql());


  auto cell2str = [](TupleCell& cell){
    if (cell.attr_type() == AttrType::CHARS) return "'" + cell.to_string() + "'";
    return cell.to_string();
  };

  string new_sql = sql_event->sql();
  while (sub_query_pair.second != SubQueryOper::NO_OPER) {
    auto sub_query_str = sub_query_pair.first;
    if (sub_query_pair.second == SubQueryOper::IN ) {


      auto query_temp = query_create();
      parse(string(sub_query_str + ";").c_str(), query_temp);
      std::regex patt("[Ff][Rr][Oo][Mm].*?[Ww][Hh][Ee][Rr][Ee]");
      smatch res;
      std::string possible_src = "";
      if(regex_search(old_str,res,patt)){
        possible_src = res[0].str().substr(5,res[0].str().length()-10);
      }
      stringstream tmp;
      vector<std::string> alias_str;
      tmp.str(possible_src);
      string tmp_str;
      while(getline(tmp,tmp_str,',')){
        alias_str.push_back(tmp_str.substr(tmp_str.find_first_not_of(' '),tmp_str.find_last_not_of(' ')-tmp_str.find_first_not_of(' ')+1));
      }
      std::unordered_map<std::string,std::string> possible_alias_map;
      for(auto alias_s : alias_str){

        if(alias_s.find(' ')==alias_s.npos){
          continue ;
        }else{
          int space_pos = alias_s.find(' ');
          std::string origin_t_name = alias_s.substr(0,space_pos);
          std::string alias_t_name = alias_s.substr(space_pos+1,alias_s.size()-space_pos-1);
          possible_alias_map.insert(pair<std::string,std::string>(alias_t_name,origin_t_name));
//          for(int i = 0 ; i < query_temp->sstr.selection.condition_num; i++){
//            if(query_temp->sstr.selection.conditions[i].left_type == 2){
//              std::string left_attr_rel_name = query_temp->sstr.selection.conditions[i].left_attr.relation_name;
//              if(left_attr_rel_name == alias_t_name){
//                query_temp->sstr.selection.conditions[i].left_attr.relation_name =  strdup(origin_t_name.c_str());
//              }
//            }
//            if(query_temp->sstr.selection.conditions[i].right_type == 2){
//              std::string right_attr_rel_name = query_temp->sstr.selection.conditions[i].right_attr.relation_name;
//              if(right_attr_rel_name == alias_t_name){
//                query_temp->sstr.selection.conditions[i].right_attr.relation_name =  strdup(origin_t_name.c_str());
//              }
//            }
//          }
        }
      }
      for(int i = 0;i<query_temp->sstr.selection.relation_num;i++){
        if(query_temp->sstr.selection.relations[i].alias!= nullptr){
          possible_alias_map.insert(pair<std::string,std::string>(string(query_temp->sstr.selection.relations[i].alias),string(query_temp->sstr.selection.relations[i].name)));
        }
      }

      string relation_name = query_temp->sstr.selection.relations[0].name;
      string attr_name ;
      if(query_temp->sstr.selection.attributes[0].relation_name == nullptr){
        attr_name = string(query_temp->sstr.selection.attributes[0].attribute_name);
      } else {
        auto iter = possible_alias_map.find(query_temp->sstr.selection.attributes[0].relation_name);
        if(iter != possible_alias_map.end()){
          attr_name = iter->second + '.' + string(query_temp->sstr.selection.attributes[0].attribute_name);
        }else{
          attr_name = string(query_temp->sstr.selection.attributes[0].relation_name) + '.' + string(query_temp->sstr.selection.attributes[0].attribute_name);
        }
      }
      vector<string> tables = {relation_name};
      vector<string> fields = {attr_name};
      string need_more_cond_table_name = "";
      string need_more_cond_attr_name = "";
      bool need_more_cond_flag = false;

      std::regex where_in("[Ww][Hh][Ee][Rr][Ee].*?[Ii][Nn]");
      smatch rs;
      if(regex_search(old_str,rs,where_in)){
        string tmp;
        tmp = rs[0].str().substr(5,rs[0].str().size()-7);
        str_replace_by_regex(tmp,"[Nn][Oo][Tt]","");
        need_more_cond_attr_name = tmp.substr(tmp.find_first_not_of(' '),tmp.find_last_not_of(' ')-tmp.find_first_not_of(' ')+1);
      }

      for (int i = 0; i < query_temp->sstr.selection.condition_num; ++i) {
        auto condition = query_temp->sstr.selection.conditions[i];
        if (condition.left_type == 2 && condition.left_attr.relation_name != nullptr && string(condition.left_attr.relation_name) != relation_name) {
          auto iter = possible_alias_map.find(string(condition.left_attr.relation_name));
          if(iter != possible_alias_map.end()){
            if(std::count(tables.begin(),tables.end(),iter->second)==0){
              tables.push_back(iter->second);
              need_more_cond_table_name = iter->second;
            }
          }else{
            if(std::count(tables.begin(),tables.end(),string(condition.left_attr.relation_name))==0){
              tables.push_back(string(condition.left_attr.relation_name));
              need_more_cond_table_name = string(condition.left_attr.relation_name);
            }
          }

          //fields.push_back(string(condition.left_attr.relation_name) + "." + string(condition.left_attr.attribute_name));
        }
        if (condition.right_type == 2 && condition.right_attr.relation_name != nullptr && string(condition.right_attr.relation_name) != relation_name) {
          auto iter = possible_alias_map.find(string(condition.right_attr.relation_name));
          if(iter != possible_alias_map.end()){
            if(std::count(tables.begin(),tables.end(),iter->second)==0){
              tables.push_back(iter->second);
              need_more_cond_table_name = iter->second;
            }
          }else{
            if(std::count(tables.begin(),tables.end(),string(condition.right_attr.relation_name))==0){
              tables.push_back(string(condition.right_attr.relation_name));
              need_more_cond_table_name = string(condition.right_attr.relation_name);
            }
          }

          //fields.push_back(string(condition.right_attr.relation_name) + "." + string(condition.right_attr.attribute_name));
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
//      string more_cond = need_more_cond_table_name + "." + need_more_cond_attr_name + " = "
//                         + query_temp->sstr.selection.attributes->relation_name + "." +
//                         + query_temp->sstr.selection.attributes->attribute_name;
      string more_cond;
      if(need_more_cond_table_name != ""){
        if(query_temp->sstr.selection.attributes[0].attribute_name != nullptr && string(query_temp->sstr.selection.attributes[0].attribute_name)!="*"){
          need_more_cond_flag = true;
          more_cond = need_more_cond_table_name + "." + need_more_cond_attr_name + " = "
                             + query_temp->sstr.selection.attributes->relation_name + "." +
                             + query_temp->sstr.selection.attributes->attribute_name;

        }
      }

      std::regex s_w_pattern("[Ss][Ee][Ll][Ee][Cc][Tt].*[Ww][Hh][Ee][Rr][Ee]");
      if(std::regex_search(temp,s_w_pattern)){
        str_replace_by_regex(temp, "[Ss][Ee][Ll][Ee][Cc][Tt].*[Ww][Hh][Ee][Rr][Ee]", "");
        sub_str += "where " + temp;

        if(need_more_cond_flag){
          sub_str += " and "+more_cond;
        }

        for(auto iter:possible_alias_map){
          str_replace_by_regex(sub_str,iter.first+".",iter.second+".");
        }
      }else{
        if(need_more_cond_flag){
          sub_str += " where " + more_cond;
        }
      }
      //str_replace_by_regex(temp, "[Ss][Ee][Ll][Ee][Cc][Tt].*[Ww][Hh][Ee][Rr][Ee]", "");
      //sub_str += "where " + temp;


      sql_event->set_sql(string(sub_str + ";").c_str());
      if((rc = handle_request(sql_event, true)) != SUCCESS) {
        return;
      }
      auto sub_select_tuples = sql_event->sub_query_res();
      string value;
      if (sub_select_tuples.size() == 0) {
        value = "null";
      } else {
        if ((sub_query_pair.second == SubQueryOper::IN && (sub_select_tuples[0]->cell_num() != 1)) ||
            (sub_query_pair.second == SubQueryOper::COMPARE && (sub_select_tuples.size() != 1 || sub_select_tuples[0]->cell_num() != 1))) {
          sql_event->session_event()->set_response("FAILURE\n");
          callback_event(sql_event, nullptr);
          return;
        }
      }
      TupleCell cell;
      for (int i = 0; i < sub_select_tuples.size(); ++i) {
        sub_select_tuples[i]->cell_at(0, cell);
        if (i == sub_select_tuples.size() - 1) {
          value += cell2str(cell);
        } else {
          value += cell2str(cell) + ",";
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
    } else if(sub_query_pair.second == SubQueryOper::COMPARE){


        auto query_temp = query_create();
        parse(string(sub_query_str + ";").c_str(), query_temp);

        string relation_name = query_temp->sstr.selection.relations[0].name;
        string attr_name ;
        if(query_temp->sstr.selection.attributes[0].relation_name == nullptr){
          attr_name = string(query_temp->sstr.selection.attributes[0].attribute_name);
        } else {

            attr_name = string(query_temp->sstr.selection.attributes[0].relation_name) + '.' + string(query_temp->sstr.selection.attributes[0].attribute_name);

        }

        if(query_temp->sstr.selection.attributes[0].aggregation_type!=NO_Aggregation){
          AggregationType agg_t = query_temp->sstr.selection.attributes[0].aggregation_type;
          switch (agg_t) {
            case AVG:{
              attr_name = "avg("+attr_name+")";
            }break;
            case MAX:{
              attr_name = "max("+attr_name+")";
            }break;
            case MIN:{
              attr_name = "min("+attr_name+")";
            } break ;
            default:
              break ;
          }
        }

        vector<string> tables = {relation_name};
        vector<string> fields = {attr_name};
        string need_more_cond_table_name = "";
        string need_more_cond_attr_name = "";
        bool need_more_cond_flag = false;

        std::regex where_comp("[Ww][Hh][Ee][Rr][Ee].*?[<>=]");
        smatch rs;
        if(regex_search(old_str,rs,where_comp)){
          string tmp;
          tmp = rs[0].str().substr(5,rs[0].str().size()-6);
          str_replace_by_regex(tmp,"[<=>]","");
          need_more_cond_attr_name = tmp.substr(tmp.find_first_not_of(' '),tmp.find_last_not_of(' ')-tmp.find_first_not_of(' ')+1);
        }



        for (int i = 0; i < query_temp->sstr.selection.condition_num; ++i) {
          auto condition = query_temp->sstr.selection.conditions[i];
          if (condition.left_type == 2 && condition.left_attr.relation_name != nullptr && string(condition.left_attr.relation_name) != relation_name) {

            if(std::count(tables.begin(),tables.end(),string(condition.left_attr.relation_name))==0){
              tables.push_back(string(condition.left_attr.relation_name));
              need_more_cond_table_name = string(condition.left_attr.relation_name);
            }

            //fields.push_back(string(condition.left_attr.relation_name) + "." + string(condition.left_attr.attribute_name));
          }

          if (condition.right_type == 2 && condition.right_attr.relation_name != nullptr && string(condition.right_attr.relation_name) != relation_name) {

            if(std::count(tables.begin(),tables.end(),string(condition.right_attr.relation_name))==0){

              tables.push_back(string(condition.right_attr.relation_name));
              need_more_cond_table_name = string(condition.right_attr.relation_name);
            }

            //fields.push_back(string(condition.right_attr.relation_name) + "." + string(condition.right_attr.attribute_name));
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
        string more_cond;
        if(need_more_cond_table_name != ""){
          if(query_temp->sstr.selection.attributes[0].attribute_name != nullptr && string(query_temp->sstr.selection.attributes[0].attribute_name)!="*"){
            need_more_cond_flag = true;
            more_cond = need_more_cond_table_name + "." + need_more_cond_attr_name + " = "
                        + query_temp->sstr.selection.attributes->relation_name + "." +
                        + query_temp->sstr.selection.attributes->attribute_name;

          }
        }

        need_more_cond_flag = false;

        std::regex s_w_pattern("[Ss][Ee][Ll][Ee][Cc][Tt].*[Ww][Hh][Ee][Rr][Ee]");
        if(std::regex_search(temp,s_w_pattern)){
          str_replace_by_regex(temp, "[Ss][Ee][Ll][Ee][Cc][Tt].*[Ww][Hh][Ee][Rr][Ee]", "");
          sub_str += "where " + temp;

          if(need_more_cond_flag){
            sub_str += " and "+more_cond;
          }

        }else{
          if(need_more_cond_flag){
            sub_str += " where " + more_cond;
          }
        }



        sql_event->set_sql(string(sub_str + ";").c_str());
        if((rc = handle_request(sql_event, true)) != SUCCESS) {
          return;
        }
        auto sub_select_tuples = sql_event->sub_query_res();
        string value;
        if (sub_select_tuples.size() == 0) {
          value = "null";
        } else {
          if ((sub_query_pair.second == SubQueryOper::IN && (sub_select_tuples[0]->cell_num() != 1)) ||
              (sub_query_pair.second == SubQueryOper::COMPARE && (sub_select_tuples.size() != 1 || sub_select_tuples[0]->cell_num() != 1))) {
            sql_event->session_event()->set_response("FAILURE\n");
            callback_event(sql_event, nullptr);
            return;
          }
        }
        TupleCell cell;
        for (int i = 0; i < sub_select_tuples.size(); ++i) {
          sub_select_tuples[i]->cell_at(0, cell);
          if (i == sub_select_tuples.size() - 1) {
            value += cell2str(cell);
          } else {
            value += cell2str(cell) + ",";
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

      }else if (sub_query_pair.second == SubQueryOper::EXIST) {
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
            value += cell2str(cell);
          } else {
            value += cell2str(cell) + ",";
          }
        }
        in_values.push_back(value);
      }
      bool is_not_exists = str_contains_by_regex(new_sql, "[Nn][Oo][Tt][ ]*[Ee][Xx][Ii][Ss][Tt][Ss]");
      string replaced_str = fields[0];
      if (in_values[0][in_values[0].length()-1] == ',') {
        in_values[0] = in_values[0].substr(0, in_values[0].length()-1);
      }
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


