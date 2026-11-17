#ifndef BANK_DATABASE_H
#define BANK_DATABASE_H

#define MAX_ACCOUNTS 100

typedef struct {
    int account_id;
    int balance;
    int total_deposit;
    int total_withdraw;
    int closed;
    char client_name[256];  // ✨ Client name'i de saklıyoruz (welcome için lazım)
} BankAccount;

// Database operations
BankAccount* find_account_by_client_name(const char* client_name);
void init_database();
int create_account(const char* client_name, int amount);
BankAccount* find_account(int account_id);
int deposit_to_account(int account_id, int amount);
int withdraw_from_account(int account_id, int amount);
int remove_account(int account_id);
void save_database_to_log(const char* log_filename);
void load_database_from_log(const char* filename);
void create_account_with_id(int id, int balance, const char* client_name);
extern int next_account_id;
#endif // BANK_DATABASE_H
