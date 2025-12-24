#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stdint.h>
#include <pthread.h>

#define MAX_ACCOUNTS 100
#define ACCOUNT_ID_LEN 20

// 帳戶資料結構
typedef struct {
    char account_id[ACCOUNT_ID_LEN];
    double balance;
    int active;  // 1 = active, 0 = inactive
    pthread_mutex_t lock;  // 每個帳戶獨立的鎖
} Account;

// 共享記憶體中的帳戶資料庫
typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int account_count;
    pthread_mutex_t db_lock;  // 全域資料庫鎖
} AccountDB;

// 交易類型
typedef enum {
    TXN_DEPOSIT,
    TXN_WITHDRAW,
    TXN_BALANCE,
    TXN_CREATE_ACCOUNT,
    TXN_UNKNOWN
} TransactionType;

// 交易請求
typedef struct {
    TransactionType type;
    char account_id[ACCOUNT_ID_LEN];
    double amount;
    uint32_t client_id;
} TransactionRequest;

// 交易回應
typedef struct {
    int success;  // 1 = success, 0 = failure
    double new_balance;
    char message[256];
} TransactionResponse;

// 函數宣告
int account_init(AccountDB *db);
int account_create(AccountDB *db, const char *account_id, double initial_balance);
int account_deposit(AccountDB *db, const char *account_id, double amount, double *new_balance);
int account_withdraw(AccountDB *db, const char *account_id, double amount, double *new_balance);
int account_get_balance(AccountDB *db, const char *account_id, double *balance);
Account* account_find(AccountDB *db, const char *account_id);
void account_cleanup(AccountDB *db);

#endif // ACCOUNT_H
