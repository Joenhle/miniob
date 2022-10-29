//
// Created by sheep on 2022/10/22.
//

#include "sort_operator.h"

RC SortOperator::open(){
  if (children_.size() != 1) {
    LOG_WARN("sort operator must has one child");
    return RC::INTERNAL;
  }
  RC rc;
  if ((rc = children_[0]->open()) != SUCCESS) {
    LOG_WARN("sort operator's children open error");
    return rc;
  }

  while ((rc = children_[0]->next()) == SUCCESS) {
    auto curr_tuple = children_[0]->current_tuple();
    TupleType tuple_t = curr_tuple->get_tuple_type();
    VTuple *temp = new VTuple;
    switch (tuple_t) {
      case TupleType::ROW:{
        RowTuple *trans = (RowTuple*)(curr_tuple);
        temp->append_row_tuple(*trans);
      }break;
      case TupleType::V:{
        temp->merge(*curr_tuple,*temp);
      }break;
      default:{
        LOG_WARN("UNKNOWN TUPLE_TYPE");
      } break ;
    }
    tuple_set.push_back(temp);
  }
  if (rc == RECORD_EOF) {
    tupleSort();
    res_iter_type = BEFORE_BEGIN;
    rc = SUCCESS;
  }
  return rc;
}

RC SortOperator::next(){
  RC rc = SUCCESS;
  if (res_iter_type == BEFORE_BEGIN){
    tuple_index = 0;
    res_iter_type = ON_WAY;
    return rc;
  }

  if(tuple_index == tuple_set.size()-1){
    return RECORD_EOF;
  }
  tuple_index++;
  return rc;
}

RC SortOperator::close()
{
  RC rc;
  if ((rc = children_[0]->close()) != SUCCESS) {
    LOG_WARN("sort operator's children close error");
    return rc;
  }
  return SUCCESS;
}

void SortOperator::tupleSort()
{
  for(auto it:tuple_set){
    Tuple4Sort tmp;
    VTuple *tmp_data = new VTuple;
    tmp_data = it;
    tmp.tuple_data = tmp_data;
    tmp.odb_fields.assign(orderby_fields_.begin(),orderby_fields_.end());
    sort_tuple_set.push_back(tmp);
  }
  sort(sort_tuple_set.begin(),sort_tuple_set.end(), compare_for_sort);
  vector<VTuple *> res;
  for(auto it:sort_tuple_set){
    res.push_back(it.tuple_data  );
  }
  tuple_set.swap(res);
}


bool compare_for_sort(Tuple4Sort arg1,Tuple4Sort arg2){
  for(auto iter : arg1.odb_fields){
    int order_flag = (iter.od_type == ASC_ORDER? 1:-1);
    TupleCell tmp_arg1_cell,tmp_arg2_cell;
    if (arg1.tuple_data->find_cell(*iter.orderby_field,tmp_arg1_cell) != SUCCESS) {
      LOG_WARN("fail to get cell");
      return 0;
    }
    if (arg2.tuple_data->find_cell(*iter.orderby_field,tmp_arg2_cell) != SUCCESS) {
      LOG_WARN("fail to get cell");
      return 0;
    }
    if(tmp_arg1_cell.is_null()||tmp_arg2_cell.is_null()){
      if(tmp_arg1_cell.is_null()&&tmp_arg2_cell.is_null()){
        continue ;
      } else{
        if(tmp_arg1_cell.is_null()){
          return false;
        }else{
          return true;
        }
      }
    }
    order_flag *=tmp_arg1_cell.compare(tmp_arg2_cell);
    if(tmp_arg1_cell.compare(tmp_arg2_cell) == 0){
      continue ;
    }else{
      return order_flag<0;
    }
  }
  return 0;
}