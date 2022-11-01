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

#pragma once

#include <string>
#include <cstring>
#include <vector>

std::string double2string(double v);
std::string vector2str(const std::vector<std::string>& v, const std::string separator= ",");
std::string stringarr2str(const char* str_arr[], int len);

std::vector<std::string> split(const std::string& str, const std::string& delim);

bool has_bit_set(int num, int loc);

int set_bit(int num, int loc, bool set);

std::string trim(std::string s);

void str_replace(std::string& s, std::string o, std::string n);

void str_replace_by_regex(std::string& s, std::string regex_str, std::string other);

void str_replace_by_regex(std::string& s, std::string regex_str, std::string(*func)(std::string));

bool str_match_by_regex(std::string& s, std::string regex_str);

bool str_contains_by_regex(std::string& s, std::string regex_str);
