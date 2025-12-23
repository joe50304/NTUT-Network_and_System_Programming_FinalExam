#ifndef STRESS_TEST_H
#define STRESS_TEST_H

/**
 * 啟動壓力測試
 * @param ip Server IP
 * @param port Server Port
 * @param num_threads 併發連線數 (例如 100)
 * @param num_requests 每個連線要發送的請求數 (例如 1000)
 */
void run_stress_test(const char *ip, int port, int num_threads, int num_requests);

#endif // STRESS_TEST_H