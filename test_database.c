#include "bank_database.h"
#include <stdio.h>

int main() {
    printf("Initializing database...\n");
    init_database();

    printf("Creating two new accounts...\n");
    int id1 = create_account(500); // Client 1 deposits 500
    int id2 = create_account(1000); // Client 2 deposits 1000

    printf("Account %d created with initial deposit 500.\n", id1);
    printf("Account %d created with initial deposit 1000.\n", id2);

    printf("Depositing 200 to account %d...\n", id1);
    deposit_to_account(id1, 200);

    printf("Withdrawing 300 from account %d...\n", id2);
    withdraw_from_account(id2, 300);

    printf("Trying to withdraw 800 from account %d (should fail)...\n", id1);
    if (withdraw_from_account(id1, 800) == -1) {
        printf("Withdraw failed as expected due to insufficient balance.\n");
    }

    printf("Withdrawing all money from account %d...\n", id1);
    withdraw_from_account(id1, 700); // 500+200=700 total, withdraw all

    printf("Removing account %d...\n", id1);
    remove_account(id1);

    printf("Saving database to log file...\n");
    save_database_to_log("AdaBank.bankLog");

    printf("Test completed.\n");

    return 0;
}
