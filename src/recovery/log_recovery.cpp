/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "log_recovery.h"
#include "logger.h"
/**
 * @description: analyze阶段，需要获得脏页表（DPT）和未完成的事务列表（ATT）
 */
void RecoveryManager::analyze() {
    while(log_manager_.flush_log_buffer()) {
        auto buffer = log_manager_.get_log_buffer();
        auto records = buffer->GetRecords();
        for(auto &record:records) {
            logs_.emplace_back(std::move(record));
        }
    }
    if(logs_.empty()) {
        LOG_DEBUG("Log File is Empty");
        return;
    }
    log_offset_ = -logs_.at(0)->lsn_;
    if(sm_manager_->master_record_end_!=INVALID_LSN) {
        auto ckpt_end = dynamic_cast<CkptEndLogRecord *>(logs_.at(sm_manager_->master_record_end_).get()); // TODO(AntiO2) 加上offset
        active_txn_table_ = std::move(ckpt_end->att_); // 初始化att
        dirty_page_table_ = std::move(ckpt_end->dpt_); // 初始化dpt
    }
    auto idx = sm_manager_->master_record_begin_!=INVALID_LSN?sm_manager_->master_record_begin_+log_offset_:0; // 从最近的ckpt开始寻找,或者从第一条记录开始
    auto length = logs_.size();
    for(;idx<length;idx++) {
        // 读取磁盘上的日志文件，并将其存储到缓存中
        auto log_record = &logs_[idx];
        auto txn_id = log_record->get()->log_tid_;
        auto lsn = log_record->get()->lsn_;
        switch (log_record->get()->log_type_) {

            case UPDATE: {
                auto update_record = dynamic_cast<UpdateLogRecord *>(log_record->get());
                std::string table_name(update_record->table_name_, update_record->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = update_record->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    dirty_page_table_[page_id] = lsn; // 记录第一个使该页面变脏的log (reclsn)
                }
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }

            case INSERT: {
                auto insert_record = dynamic_cast<InsertLogRecord *>(log_record->get());
                std::string table_name(insert_record->table_name_, insert_record->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd=table->GetFd(), .page_no=insert_record->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    dirty_page_table_[page_id] = lsn; // 记录第一个使该页面变脏的log (reclsn)
                }
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }

            case DELETE: {
                auto delete_record = dynamic_cast<DeleteLogRecord *>(log_record->get());
                std::string table_name(delete_record->table_name_, delete_record->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd=table->GetFd(), .page_no=delete_record->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    dirty_page_table_[page_id] = lsn; // 记录第一个使该页面变脏的log (reclsn)
                }
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }
            case BEGIN: {
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }
            case COMMIT: {
                active_txn_table_.erase(txn_id);
                break;
            }
            case ABORT: {
                active_txn_table_.erase(txn_id);
                break;
            }
            case CLR_INSERT: {
                auto insert_record = dynamic_cast<CLR_Insert_Record *>(log_record->get());
                std::string table_name(insert_record->table_name_, insert_record->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd=table->GetFd(), .page_no=insert_record->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    dirty_page_table_[page_id] = lsn; // 记录第一个使该页面变脏的log (reclsn)
                }
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }
            case CLR_DELETE: {
                auto delete_record = dynamic_cast<CLR_DELETE_RECORD *>(log_record->get());
                std::string table_name(delete_record->table_name_, delete_record->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd=table->GetFd(), .page_no=delete_record->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    dirty_page_table_[page_id] = lsn; // 记录第一个使该页面变脏的log (reclsn)
                }
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }
            case CLR_UPDATE: {
                auto update_record = dynamic_cast<CLR_UPDATE_RECORD *>(log_record->get());
                std::string table_name(update_record->table_name_, update_record->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = update_record->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    dirty_page_table_[page_id] = lsn; // 记录第一个使该页面变脏的log (reclsn)
                }
                active_txn_table_[txn_id] = lsn;  // 更新该事务的last lsn
                break;
            }
            case CKPT_BEGIN: {
                // 无事发生
                break;
            }
            case CKPT_END: {
                break;
            }
        }
    }
    log_manager_.set_global_lsn(idx-log_offset_); // 设置global lsn
}

/**

@description: 重做所有未落盘的操作
*/
void RecoveryManager::redo() {
    if(dirty_page_table_.empty()) {
        return;
    }
    auto dpt_iter = dirty_page_table_.begin();
    lsn_t redo_lsn = dpt_iter->second;
    dpt_iter++;
    while (dpt_iter!=dirty_page_table_.end()) {
        // 找到redo的起点
        redo_lsn = std::min(redo_lsn,dpt_iter->second);
        dpt_iter++;
    }
    LogRecord* log;
    while((log = get_log_by_lsn(redo_lsn))!= nullptr) {
        auto lsn = log->lsn_;
        switch (log->log_type_) {

            case UPDATE: {
                auto update_log = dynamic_cast<UpdateLogRecord*>(log);
                std::string table_name(update_log->table_name_, update_log->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = update_log->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    // 如果不在脏页表中，不需要重做
                    break;
                }
                auto page = buffer_pool_manager_->fetch_page(page_id);
                if(page->get_page_lsn() >= lsn) {
                    // 如果已经被持久化，不需要更新
                    break;
                }
                auto fh = sm_manager_->fhs_.at(table_name).get(); // check(AntiO2) 是否会造成崩溃，比如表已经被删除了的情况
                fh->update_record(update_log->rid_,update_log->after_update_value_.data,lsn);
                buffer_pool_manager_->unpin_page(page_id, true);

                break;
            }
            case INSERT: {
                    auto insert_log = dynamic_cast<InsertLogRecord*>(log);
                    std::string table_name(insert_log->table_name_, insert_log->table_name_size_);
                    auto table = sm_manager_->fhs_[table_name].get();
                    auto page_id = PageId{.fd = table->GetFd(), .page_no = insert_log->rid_.page_no};
                    if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                        // 如果不在脏页表中，不需要重做
                        break;
                    }
                    auto page = buffer_pool_manager_->fetch_page(page_id);
                    if(page->get_page_lsn() >= lsn) {
                        // 如果已经被持久化，不需要更新
                        break;
                    }
                    auto fh = sm_manager_->fhs_.at(table_name).get(); // check(AntiO2) 是否会造成崩溃，比如表已经被删除了的情况
                    fh->insert_record(insert_log->rid_, insert_log->insert_value_.data,lsn);
                    buffer_pool_manager_->unpin_page(page_id, true);
                    break;
            }
            case DELETE: {
                auto delete_log = dynamic_cast<DeleteLogRecord*>(log);
                std::string table_name(delete_log->table_name_, delete_log->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = delete_log->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    // 如果不在脏页表中，不需要重做
                    break;
                }
                auto page = buffer_pool_manager_->fetch_page(page_id);
                if(page->get_page_lsn() >= lsn) {
                    // 如果已经被持久化，不需要更新
                    break;
                }
                auto fh = sm_manager_->fhs_.at(table_name).get(); // check(AntiO2) 是否会造成崩溃，比如表已经被删除了的情况
                fh->delete_record(delete_log->rid_,lsn);
                buffer_pool_manager_->unpin_page(page_id, true);
                break;
            }
            case CLR_INSERT:{
                auto insert_log = dynamic_cast<CLR_Insert_Record*>(log);
                std::string table_name(insert_log->table_name_, insert_log->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = insert_log->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    // 如果不在脏页表中，不需要重做
                    break;
                }
                auto page = buffer_pool_manager_->fetch_page(page_id);
                if(page->get_page_lsn() >= lsn) {
                    // 如果已经被持久化，不需要更新
                    break;
                }
                auto fh = sm_manager_->fhs_.at(table_name).get(); // check(AntiO2) 是否会造成崩溃，比如表已经被删除了的情况
                fh->insert_record(insert_log->rid_, insert_log->insert_value_.data,lsn);
                buffer_pool_manager_->unpin_page(page_id, true);
                break;
            }
            case CLR_DELETE:  {
                auto delete_log = dynamic_cast<CLR_DELETE_RECORD*>(log);
                std::string table_name(delete_log->table_name_, delete_log->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = delete_log->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    // 如果不在脏页表中，不需要重做
                    break;
                }
                auto page = buffer_pool_manager_->fetch_page(page_id);
                if(page->get_page_lsn() >= lsn) {
                    // 如果已经被持久化，不需要更新
                    break;
                }
                auto fh = sm_manager_->fhs_.at(table_name).get(); // check(AntiO2) 是否会造成崩溃，比如表已经被删除了的情况
                fh->delete_record(delete_log->rid_,lsn);
                buffer_pool_manager_->unpin_page(page_id, true);
                break;
            }

            case CLR_UPDATE: {
                auto update_log = dynamic_cast<CLR_UPDATE_RECORD*>(log);
                std::string table_name(update_log->table_name_, update_log->table_name_size_);
                auto table = sm_manager_->fhs_[table_name].get();
                auto page_id = PageId{.fd = table->GetFd(), .page_no = update_log->rid_.page_no};
                if (dirty_page_table_.find(page_id) == dirty_page_table_.end()) {
                    // 如果不在脏页表中，不需要重做
                    break;
                }
                auto page = buffer_pool_manager_->fetch_page(page_id);
                if(page->get_page_lsn() >= lsn) {
                    // 如果已经被持久化，不需要更新
                    break;
                }
                auto fh = sm_manager_->fhs_.at(table_name).get(); // check(AntiO2) 是否会造成崩溃，比如表已经被删除了的情况
                fh->update_record(update_log->rid_,update_log->after_update_value_.data,lsn);
                buffer_pool_manager_->unpin_page(page_id, true);
                break;
            }
//            case CKPT_BEGIN:
//                break;
//            case CKPT_END:
//                break;
//            case BEGIN:
//                break;
//            case COMMIT:
//                break;
//            case ABORT:
//                break;
            default:
                break;
        }
        redo_lsn++;
    }

}

/**
 * @description: 回滚未完成的事务
 */
void RecoveryManager::undo() {
    for(auto txn:active_txn_table_) {
        auto undo_lsn = txn.second;
        auto tid = txn.first;
        if(undo_lsn==INVALID_LSN) {
            LOG_ERROR("undo lsn is invalid");
            continue;
        }
        auto log = get_log_by_lsn(undo_lsn);
        while(log!= nullptr) {
            switch (log->log_type_) {
                case UPDATE: {
                    auto update_log = dynamic_cast<UpdateLogRecord *>(log);
                    std::string table_name(update_log->table_name_, update_log->table_name_size_);
                    auto table = sm_manager_->fhs_[table_name].get();
                    auto page_id = PageId{.fd = table->GetFd(), .page_no = update_log->rid_.page_no};
                    CLR_UPDATE_RECORD clr_record(update_log->log_tid_,
                                                 update_log->after_update_value_, update_log->before_update_value_,
                                                 update_log->rid_,table_name,update_log->lsn_,update_log->prev_lsn_); // 注意这里CLR补偿记录after和before value顺序 redo和undo是反的
                    log_manager_.add_log_to_buffer(&clr_record);
                    auto fh = sm_manager_->fhs_.at(table_name).get();
                    fh->update_record(update_log->rid_,update_log->before_update_value_.data,clr_record.lsn_);
                    undo_lsn = log->prev_lsn_;
                    break;
                }

                case INSERT: {
                    auto insert_log = dynamic_cast<InsertLogRecord *>(log);
                    std::string table_name(insert_log->table_name_, insert_log->table_name_size_);
                    auto table = sm_manager_->fhs_[table_name].get();
                    CLR_DELETE_RECORD clr_record(insert_log->log_tid_, insert_log->insert_value_,
                                                 insert_log->rid_,table_name,insert_log->lsn_,insert_log->prev_lsn_); // 注意这里CLR补偿记录after和before value顺序 redo和undo是反的
                    log_manager_.add_log_to_buffer(&clr_record);
                    auto fh = sm_manager_->fhs_.at(table_name).get();
                    fh->delete_record(insert_log->rid_,clr_record.lsn_);
                    undo_lsn = log->prev_lsn_;
                    break;
                }
                case DELETE: {
                    auto delete_log = dynamic_cast<DeleteLogRecord*>(log);
                    std::string table_name(delete_log->table_name_, delete_log->table_name_size_);
                    auto table = sm_manager_->fhs_[table_name].get();
                    CLR_Insert_Record clr_record(delete_log->log_tid_,  delete_log->delete_value_,
                                                 delete_log->rid_,table_name, delete_log->lsn_, delete_log->prev_lsn_); // 注意这里CLR补偿记录after和before value顺序 redo和undo是反的
                    log_manager_.add_log_to_buffer(&clr_record);
                    auto fh = sm_manager_->fhs_.at(table_name).get();
                    fh->insert_record(delete_log->rid_,delete_log->delete_value_.data,clr_record.lsn_);
                    undo_lsn = log->prev_lsn_;
                    break;
                }

                case CLR_INSERT: {
                    auto clr_log = dynamic_cast<CLR_Insert_Record*>(log);
                    undo_lsn = clr_log->undo_next_;
                    break;
                }

                case CLR_DELETE: {
                    auto clr_log = dynamic_cast<CLR_DELETE_RECORD*>(log);
                    undo_lsn = clr_log->undo_next_;
                    break;
                }
                case CLR_UPDATE: {
                    auto clr_log = dynamic_cast<CLR_Insert_Record*>(log);
                    undo_lsn = clr_log->undo_next_;
                    break;
                }
                case BEGIN:{
                    undo_lsn = INVALID_LSN;
                    break;
                }
                case COMMIT: {
                    undo_lsn = log->prev_lsn_;
                    break;
                }
                case ABORT: {
                    undo_lsn = log->prev_lsn_;
                    break;
                }

                default: {
                    LOG_ERROR("Error log type while undoing");
                }
            }
            log = get_log_by_lsn(undo_lsn);
        }
    }
}

LogRecord *RecoveryManager::get_log_by_lsn(lsn_t lsn) {
    auto idx = lsn + log_offset_;
    if(idx >= logs_.size()||idx < 0) {
        return nullptr;
    }
    return logs_.at(idx).get();
}
