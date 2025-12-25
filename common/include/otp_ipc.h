#ifndef OTP_IPC_H
#define OTP_IPC_H

#define OTP_PORT 8889
#define OTP_IP "127.0.0.1"

// 操作類型
#define OTP_OP_GENERATE 1
#define OTP_OP_VERIFY   2

// 請求封包 (Bank Server -> OTP Server)
typedef struct {
    int op_code;        // 1=Gen, 2=Verify
    char account[32];   // 帳號
    char otp_code[8];   // 驗證時填入，請求時留空
} OtpIpcRequest;

// 回應封包 (OTP Server -> Bank Server)
typedef struct {
    int status;         // 1=Success, 0=Fail
    char otp_code[8];   // Gen 成功時回傳
    char message[64];   // 訊息
} OtpIpcResponse;

#endif