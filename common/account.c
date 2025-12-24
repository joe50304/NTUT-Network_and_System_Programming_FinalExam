#include "account.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 初始化帳戶資料庫
int account_init(AccountDB *db) {
    if (!db) return -1;
    
    memset(db, 0, sizeof(AccountDB));
    db->account_count = 0;
    
    // 初始化全域鎖
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);  // 支援跨行程
    pthread_mutex_init(&db->db_lock, &attr);
    pthread_mutexattr_destroy(&attr);
    
    // 初始化每個帳戶的鎖
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&db->accounts[i].lock, &attr);
        pthread_mutexattr_destroy(&attr);
        db->accounts[i].active = 0;
    }
    
    return 0;
}

// 尋找帳戶
Account* account_find(AccountDB *db, const char *account_id) {
    if (!db || !account_id) return NULL;
    
    for (int i = 0; i < db->account_count; i++) {
        if (db->accounts[i].active && 
            strcmp(db->accounts[i].account_id, account_id) == 0) {
            return &db->accounts[i];
        }
    }
    return NULL;
}

// 建立新帳戶
int account_create(AccountDB *db, const char *account_id, double initial_balance) {
    if (!db || !account_id) return -1;
    if (initial_balance < 0) return -1;
    
    pthread_mutex_lock(&db->db_lock);
    
    // 檢查帳戶是否已存在
    if (account_find(db, account_id) != NULL) {
        pthread_mutex_unlock(&db->db_lock);
        return -2;  // Account already exists
    }
    
    // 檢查是否已滿
    if (db->account_count >= MAX_ACCOUNTS) {
        pthread_mutex_unlock(&db->db_lock);
        return -3;  // Database full
    }
    
    // 建立新帳戶
    Account *acc = &db->accounts[db->account_count];
    strncpy(acc->account_id, account_id, ACCOUNT_ID_LEN - 1);
    acc->account_id[ACCOUNT_ID_LEN - 1] = '\0';
    acc->balance = initial_balance;
    acc->active = 1;
    db->account_count++;
    
    pthread_mutex_unlock(&db->db_lock);
    
    printf("[ACCOUNT] Created account %s with initial balance %.2f\n", 
           account_id, initial_balance);
    return 0;
}

// 存款
int account_deposit(AccountDB *db, const char *account_id, double amount, double *new_balance) {
    if (!db || !account_id || amount <= 0) return -1;
    
    pthread_mutex_lock(&db->db_lock);
    Account *acc = account_find(db, account_id);
    
    if (!acc) {
        pthread_mutex_unlock(&db->db_lock);
        return -2;  // Account not found
    }
    
    // 鎖定該帳戶（兩階段鎖定）
    pthread_mutex_lock(&acc->lock);
    pthread_mutex_unlock(&db->db_lock);
    
    // 執行交易
    acc->balance += amount;
    if (new_balance) *new_balance = acc->balance;
    
    printf("[ACCOUNT] Deposit %.2f to %s, new balance: %.2f\n", 
           amount, account_id, acc->balance);
    
    pthread_mutex_unlock(&acc->lock);
    return 0;
}

// 提款
int account_withdraw(AccountDB *db, const char *account_id, double amount, double *new_balance) {
    if (!db || !account_id || amount <= 0) return -1;
    
    pthread_mutex_lock(&db->db_lock);
    Account *acc = account_find(db, account_id);
    
    if (!acc) {
        pthread_mutex_unlock(&db->db_lock);
        return -2;  // Account not found
    }
    
    // 鎖定該帳戶
    pthread_mutex_lock(&acc->lock);
    pthread_mutex_unlock(&db->db_lock);
    
    // 檢查餘額（防止透支）
    if (acc->balance < amount) {
        pthread_mutex_unlock(&acc->lock);
        return -3;  // Insufficient funds
    }
    
    // 執行交易
    acc->balance -= amount;
    if (new_balance) *new_balance = acc->balance;
    
    printf("[ACCOUNT] Withdraw %.2f from %s, new balance: %.2f\n", 
           amount, account_id, acc->balance);
    
    pthread_mutex_unlock(&acc->lock);
    return 0;
}

// 查詢餘額
int account_get_balance(AccountDB *db, const char *account_id, double *balance) {
    if (!db || !account_id || !balance) return -1;
    
    pthread_mutex_lock(&db->db_lock);
    Account *acc = account_find(db, account_id);
    
    if (!acc) {
        pthread_mutex_unlock(&db->db_lock);
        return -2;  // Account not found
    }
    
    pthread_mutex_lock(&acc->lock);
    pthread_mutex_unlock(&db->db_lock);
    
    *balance = acc->balance;
    
    printf("[ACCOUNT] Balance query for %s: %.2f\n", account_id, *balance);
    
    pthread_mutex_unlock(&acc->lock);
    return 0;
}

// 清理資源
void account_cleanup(AccountDB *db) {
    if (!db) return;
    
    pthread_mutex_destroy(&db->db_lock);
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        pthread_mutex_destroy(&db->accounts[i].lock);
    }
}
