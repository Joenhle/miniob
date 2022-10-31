//
// Created by sheep on 2022/10/17.
//

#include "rc.h"
#include "update_operator.h"
#include "sql/stmt/update_stmt.h"
RC UpdateOperator::open() {
  if (children_.size() != 1) {
    LOG_WARN("update operator must has 1 child");
    return INTERNAL;
  }

  Operator *child = children_[0];
  RC rc = child->open();
  if (rc != SUCCESS) {
    LOG_WARN("Failed to open child operator: %s.", strrc(rc));
    return rc;
  }

  Table *table = update_stmt_->table();
  while (SUCCESS == (rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("Failed to get current record: %s.", strrc(rc));
      return rc;
    }
    RowTuple *rowTuple = static_cast<RowTuple*>(tuple);
    Record &record = rowTuple->record();
    int val_amount = update_stmt_->value_amount();
    vector<std::string > to_update_attr_name = update_stmt_->update_attr();
    vector<Value *> to_update_attr_val = update_stmt_->values();
    for(int i = 0 ;i < val_amount ; i++){
      const char * attr_name = to_update_attr_name[i].c_str();
      rc = table->update_record(nullptr,&record,attr_name,to_update_attr_val[i]);
      if (rc != SUCCESS) {
        LOG_WARN("Failed to update record: %s.", strrc(rc));
        return rc;
      }
    }
    //for(int i = 0 ;i<update_stmt_->)
    //table->update_record(nullptr, &record, update_stmt_->update_attr(), update_stmt_->values());
    if (rc != SUCCESS) {
      LOG_WARN("Failed to update record: %s.", strrc(rc));
      return rc;
    }
  }
  return SUCCESS;
}

RC UpdateOperator::next() {
  return RC::RECORD_EOF;
}

RC UpdateOperator::close() {
  children_[0]->close();
  return RC::SUCCESS;
}