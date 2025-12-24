#ifndef IPC_H
#define IPC_H

#include "account.h"
#include <sys/types.h>

#define SHM_KEY 0x12345678
#define SEM_KEY 0x87654321

// IPC 控制結構
typedef struct {
    int shm_id;
    int sem_id;
    AccountDB *db;
} IPCContext;

// 函數宣告
int ipc_init_server(IPCContext *ctx);
int ipc_attach_client(IPCContext *ctx);
void ipc_cleanup(IPCContext *ctx, int is_server);
AccountDB* ipc_get_db(IPCContext *ctx);

#endif // IPC_H
