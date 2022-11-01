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
// Created by wangyunlai on 2022/9/28
//

#include <string>
#include <vector>
#include <string.h>
#include <sstream>
#include "util/util.h"
#include <regex>

std::string double2string(double v)
{
  char buf[256];
  snprintf(buf, sizeof(buf), "%.2f", v);
  size_t len = strlen(buf);
  while (buf[len - 1] == '0') {
    len--;
      
  }
  if (buf[len - 1] == '.') {
    len--;
  }

  return std::string(buf, len);
}

std::vector<std::string> split(const std::string& str, const std::string& delim) {
  std::vector<std::string> res;
  if("" == str) return res;
  //先将要切割的字符串从string类型转换为char*类型
  char * strs = new char[str.length() + 1] ; //不要忘了
  strcpy(strs, str.c_str());

  char * d = new char[delim.length() + 1];
  strcpy(d, delim.c_str());

  char *p = strtok(strs, d);
  while(p) {
    std::string s = p; //分割得到的字符串转换为string类型
    res.push_back(s); //存入结果数组
    p = strtok(NULL, d);
  }

  return res;
}

bool has_bit_set(int num, int loc) {
  int temp = 1 << loc;
  return (num & temp) != 0;
}

int set_bit(int num, int loc, bool set) {
  if (set) {
    return (num | (1 << loc));
  } else {
    return (num & (~(1 << loc)));
  }
}

std::string trim(std::string s) {
  if (s.empty()) {
    return s;
  }
  s.erase(0, s.find_first_not_of(" "));
  s.erase(s.find_last_not_of(" ")+1);
  return s;
}
std::string stringarr2str(const char *str_arr[], int len)
{
  std::vector<std::string> v(str_arr, str_arr+len);
  return vector2str(v);
}
std::string vector2str(const std::vector<std::string> &v, const std::string separator)
{
  std::stringstream ss;
  for(size_t i = 0, sz = v.size(); i < sz; i++){
    if(i != 0){
      ss << separator;
    }
    ss << v[i];
  }
  return ss.str();
}

void str_replace(std::string& s, std::string o, std::string n) {
  while (s.find(o) != s.npos) {
    s = s.replace(s.find(o), o.length(), n);
  }
}

void str_replace_by_regex(std::string& s, std::string regex_str, std::string other) {
  try {
    std::regex rule(regex_str);
    s = regex_replace(s, rule, other);
  } catch (std::regex_error &e) {
    return;
  }
}

void str_replace_by_regex(std::string& s, std::string regex_str, std::string(*func)(std::string)) {
  std::smatch result;
  std::regex pattern(regex_str);
  std::string::const_iterator iterStart = s.begin();
  std::string::const_iterator iterEnd = s.end();
  std::string res = s;
  while (std::regex_search(iterStart, iterEnd, result, pattern)) {
    auto key = result[0];
    auto before = key.first-1, right = key.second;
    bool left_brace = false, right_brace = false;
    while (before >= s.begin()) {
      if (*before == ' ') {
        before--;
        continue;
      }
      left_brace = (*before == '(');
      break;
    }
    while (right <= s.end()) {
      if (*right == ' ') {
        right++;
        continue;
      }
      right_brace = (*right == ')');
      break;
    }
    if (!left_brace || !right_brace) {
      auto value = func(key);
      str_replace(res, key, value);
    }
    iterStart = result[0].second;
  }
  s = res;
}

bool str_contains_by_regex(std::string& s, std::string regex_str) {
  bool res = false;
  try {
    std::regex rule(regex_str);
    res = std::regex_search(s, rule);
  } catch (std::regex_error &e) {
    return false;
  }
  return res;
}