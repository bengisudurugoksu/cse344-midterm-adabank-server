#include "bank_database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

BankAccount accounts[MAX_ACCOUNTS];
int account_count = 0;
int next_account_id = 1;

void load_database_from_log(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("[Database] Failed to open log file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue; // Yorumları ve boş satırları atla
        }

        int id = -1, deposited = 0, withdrawn = 0, balance = 0; char client_name[256] = "";
        if (sscanf(line, "BankID_%d D %d W %d %d C %s", &id, &deposited, &withdrawn, &balance, client_name) == 5)  {
            if (account_count >= MAX_ACCOUNTS) {
                printf("[Database] Maximum account limit reached!\n");
                break;
            }

            accounts[account_count].account_id = id;
            accounts[account_count].balance = balance;
            accounts[account_count].total_deposit = deposited;
            accounts[account_count].total_withdraw = withdrawn;
            strcpy(accounts[account_count].client_name, client_name);
            account_count++;

            if (id >= next_account_id) {
                next_account_id = id + 1; // ID'yi güncelle
            }
        }
    }

    fclose(file);
}

BankAccount* find_account(int account_id) {
    for (int i = 0; i < account_count; ++i) {
        if (accounts[i].account_id == account_id) {
            return &accounts[i];
        }
    }
    return NULL;
}
BankAccount* find_account_by_client_name(const char* client_name) {
    for (int i = 0; i < account_count; i++) {
        if (strcmp(accounts[i].client_name, client_name) == 0 && accounts[i].closed == 0) {
            return &accounts[i];
        }
    }
    return NULL;
}

int deposit_to_account(int account_id, int amount) {
    BankAccount* acc = find_account(account_id);
    if (acc == NULL) return -1;
    acc->balance += amount;
    acc->total_deposit += amount;
    return 0;
}

int withdraw_from_account(int account_id, int amount) {
    BankAccount* acc = find_account(account_id);
    if (acc == NULL) return -1;

    if (acc->balance >= amount) {
        acc->balance -= amount;
        acc->total_withdraw += amount;
        return 0;
    }

    return -1;
}


int remove_account(int account_id) {
    for (int i = 0; i < account_count; ++i) {
        if (accounts[i].account_id == account_id) {
            for (int j = i; j < account_count - 1; ++j) {
                accounts[j] = accounts[j + 1];
            }
            account_count--;
            return 0;
        }
    }
    return -1;
}

void save_database_to_log(const char* log_filename) {
    FILE* file = fopen(log_filename, "w");
    if (file == NULL) {
        perror("[Database] Failed to open log file for writing");
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    fprintf(file, "# Adabank Log file updated @%02d:%02d %s %d %d\n",
            t->tm_hour, t->tm_min,
            (char*[]){"January","February","March","April","May","June","July","August","September","October","November","December"}[t->tm_mon],
            t->tm_mday,
            t->tm_year + 1900);

    for (int i = 0; i < account_count; ++i) {
        fprintf(file, "BankID_%02d D %d W %d %d C %s\n", 
            accounts[i].account_id,
            accounts[i].total_deposit,
            accounts[i].total_withdraw,
            accounts[i].balance,
            accounts[i].client_name);
    }

    fprintf(file, "## end of log.\n");
    fclose(file);
}


void create_account_with_id(int id, int balance, const char* client_name) {
    if (account_count >= MAX_ACCOUNTS) {
        printf("[Database] Maximum account limit reached!\n");
        return;
    }

    accounts[account_count].account_id = id;
    accounts[account_count].balance = balance;
    accounts[account_count].total_deposit = balance;
    accounts[account_count].total_withdraw = 0;
    accounts[account_count].closed = 0;
    strncpy(accounts[account_count].client_name, client_name, 255); // ✨
    accounts[account_count].client_name[255] = '\0'; // null terminate
    account_count++;
}

void init_database() {
    account_count = 0;
    next_account_id = 1;
    memset(accounts, 0, sizeof(accounts));
}

int create_account(const char* client_name, int amount) {
    int id = next_account_id++;
    create_account_with_id(id, amount, client_name);
    return id;
}
