# High-Concurrency Banking Transaction System

高效能銀行交易系統專案，旨在展示對作業系統機制 (Process/Thread/IPC) 與軟體架構設計的掌握。本系統具備高併發處理能力、自定義應用層協定以及容錯機制。

## 功能特色 (Key Features)

- **高併發伺服器 (High Concurrency Server)**: 採用 **Multi-process (Preforking)** 架構，結合 **IPC (Shared Memory)** 實現高效能請求處理與資源共享。
- **安全性 (Security)**:
    - 支援 **TLS/SSL** 加密連線。
    - 整合 **OTP (One-Time Password)** 雙因子驗證機制，透過獨立的 OTP Microservice 處理。
- **自定義協定 (Custom Protocol)**: 設計專屬應用層協定 (Application Layer Protocol)，包含 Header, OpCode, Checksum 與 Payload。
- **壓力測試 (Stress Testing)**: 內建 **Multi-threaded** 壓力測試客戶端，可模擬 100+ 併發連線進行負載測試。
- **模組化設計**: 核心功能封裝於靜態函式庫 (`libcommon.a`)。

## 系統架構

1. **Banking Server**: 主伺服器，負責帳務邏輯。使用 Prefork 模式預先建立 Worker Processes。
2. **OTP Server**: 獨立運行的微服務，負責產生與驗證一次性密碼。
3. **Account DB**: 透過 Shared Memory 實作的 In-memory 資料庫，並使用 Mutex/Semaphore 確保資料一致性。
4. **Clients**: 包含一般互動式客戶端 (`banking_client`) 與壓力測試客戶端 (`stress_client`)。

## 建置需求 (Prerequisites)

- Operating System: Linux (or WSL)
- Compiler: GCC
- Libraries: OpenSSL (`libssl-dev`)

## 編譯說明 (Build)

所有元件皆已透過 Makefile 整合。

```bash
# 清理並編譯所有目標
make clean && make all
```

編譯產出物將位於 `bin/` 目錄：
- `bin/banking_server`
- `bin/banking_client`
- `bin/stress_client`
- `bin/otp_server`
- `bin/libcommon.a`

## 執行指南 (Usage)

請依序開啟終端機執行各項服務：

### 1. 啟動 OTP Server
首先啟動 OTP 服務 (Port 8889)。
```bash
./bin/otp_server
```

### 2. 啟動 Banking Server
啟動主要銀行伺服器 (Port 8888)。
```bash
# Usage: ./banking_server <port> <verify_client>
./bin/banking_server 8888 0
```

### 3. 執行客戶端

#### 選項 A: 壓力測試 (Stress Test)
模擬高併發交易 (預設 100 執行緒)。
```bash
# Usage: ./stress_client <ip> <port> <threads> <requests> <verify_cert>
./bin/stress_client 127.0.0.1 8888 100 100 0
```

#### 選項 B: 互動式客戶端 (Interactive Client)
手動操作各項功能。
```bash
./bin/banking_client 127.0.0.1 8888 0
```

## 目錄結構
- `server/`: Banking Server 核心實作
- `client/`: 互動式 Client 實作 (包含組員實作部分)
- `stress_test/`: 壓力測試 Client 實作
- `otp_server/`: OTP 服務實作
- `common/`: 共用 Header 與 Source Code (封裝為 libcommon)

---
**Note**: 本專案為系統程式設計期末專案實作。
