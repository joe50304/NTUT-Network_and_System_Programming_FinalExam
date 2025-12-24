#include "ipc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Server 端初始化 IPC
int ipc_init_server(IPCContext *ctx) {
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(IPCContext));
    
    // 建立共享記憶體
    ctx->shm_id = shmget(SHM_KEY, sizeof(AccountDB), IPC_CREAT | IPC_EXCL | 0666);
    if (ctx->shm_id < 0) {
        if (errno == EEXIST) {
            // 已存在，清除舊的
            printf("[IPC] Removing existing shared memory...\n");
            int old_shm = shmget(SHM_KEY, 0, 0);
            if (old_shm >= 0) {
                shmctl(old_shm, IPC_RMID, NULL);
            }
            // 重新建立
            ctx->shm_id = shmget(SHM_KEY, sizeof(AccountDB), IPC_CREAT | 0666);
        }
        
        if (ctx->shm_id < 0) {
            perror("[IPC] shmget failed");
            return -1;
        }
    }
    
    // 附加共享記憶體
    ctx->db = (AccountDB *)shmat(ctx->shm_id, NULL, 0);
    if (ctx->db == (void *)-1) {
        perror("[IPC] shmat failed");
        shmctl(ctx->shm_id, IPC_RMID, NULL);
        return -1;
    }
    
    // 初始化帳戶資料庫
    if (account_init(ctx->db) < 0) {
        fprintf(stderr, "[IPC] Failed to initialize account database\n");
        shmdt(ctx->db);
        shmctl(ctx->shm_id, IPC_RMID, NULL);
        return -1;
    }
    
    printf("[IPC] Shared memory initialized (ID: %d, Size: %lu bytes)\n", 
           ctx->shm_id, sizeof(AccountDB));
    
    return 0;
}

// Client 端附加 IPC
int ipc_attach_client(IPCContext *ctx) {
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(IPCContext));
    
    // 取得現有的共享記憶體
    ctx->shm_id = shmget(SHM_KEY, sizeof(AccountDB), 0666);
    if (ctx->shm_id < 0) {
        perror("[IPC] shmget failed (client)");
        return -1;
    }
    
    // 附加共享記憶體
    ctx->db = (AccountDB *)shmat(ctx->shm_id, NULL, 0);
    if (ctx->db == (void *)-1) {
        perror("[IPC] shmat failed (client)");
        return -1;
    }
    
    printf("[IPC] Attached to shared memory (ID: %d)\n", ctx->shm_id);
    
    return 0;
}

// 取得資料庫指標
AccountDB* ipc_get_db(IPCContext *ctx) {
    return ctx ? ctx->db : NULL;
}

// 清理 IPC 資源
void ipc_cleanup(IPCContext *ctx, int is_server) {
    if (!ctx) return;
    
    if (ctx->db && ctx->db != (void *)-1) {
        if (is_server) {
            account_cleanup(ctx->db);
        }
        shmdt(ctx->db);
        printf("[IPC] Detached from shared memory\n");
    }
    
    if (is_server && ctx->shm_id >= 0) {
        shmctl(ctx->shm_id, IPC_RMID, NULL);
        printf("[IPC] Removed shared memory\n");
    }
}
