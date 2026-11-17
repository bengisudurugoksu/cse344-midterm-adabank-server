#include "bank_database.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

// Shared Memory ve Semaphore Key'lerini tanımlayın
#define SHM_KEY 1234
#define SEM_KEY 5678

#define SERVER_FIFO_NAME "ServerFIFO"
#define MAX_TELLERS 100
#define MAX_BATCH 100

typedef struct {
    char client_name[256];
    pid_t client_pid;
    int account_id;
} ClientMapping;

ClientMapping client_mappings[MAX_BATCH];
int client_mapping_count = 0;

int find_client_account_id(const char* client_name, pid_t client_pid) {
    for (int i = 0; i < client_mapping_count; i++) {
        if (strcmp(client_mappings[i].client_name, client_name) == 0 &&
            client_mappings[i].client_pid == client_pid) {
            return client_mappings[i].account_id;
        }
    }
    return -1;
}

void add_client_mapping(const char* client_name, pid_t client_pid, int account_id) {
    for (int i = 0; i < client_mapping_count; i++) {
        if (strcmp(client_mappings[i].client_name, client_name) == 0 &&
            client_mappings[i].client_pid == client_pid) {
            return; // zaten var
        }
    }
    strcpy(client_mappings[client_mapping_count].client_name, client_name);
    client_mappings[client_mapping_count].client_pid = client_pid;
    client_mappings[client_mapping_count].account_id = account_id;
    client_mapping_count++;
}


typedef struct {
    char operation[256];  // Yapılacak işlem
    int account_id;
    int amount;
    int result;  // Sonuç
} shared_data;


int shm_id;
shared_data* shm_ptr;
int sem_id;

void create_shared_memory() {
    shm_id = shmget(SHM_KEY, sizeof(shared_data) * 100, IPC_CREAT | 0666);

    if (shm_id < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }
    
    shm_ptr = shmat(shm_id, NULL, 0);
    if (shm_ptr == (void*) -1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }
    printf("Shared memory attached at address %p\n", shm_ptr);
}

void create_semaphore() {
    // MAX_TELLERS kadar semafor oluşturuyoruz
    sem_id = semget(SEM_KEY, MAX_TELLERS, IPC_CREAT | 0600);  // semget ile MAX_TELLERS kadar semafor oluşturuyoruz
    if (sem_id == -1) {
        printf("semget failed with errno = %d\n", errno); // errno değerini yazdır
        perror("semget failed");
        exit(EXIT_FAILURE);
    }
    

    // Her semaforu başlatıyoruz, değerini 1 yapıyoruz
    for (int i = 0; i < MAX_TELLERS; i++) {
        if (semctl(sem_id, i, SETVAL, 1) == -1) {
            printf("semctl SETVAL failed for semaphore %d with errno = %d\n", i, errno);
            perror("semctl failed");
            exit(EXIT_FAILURE);
        }
        
    }

    // Semaforları doğruluyoruz
    for (int i = 0; i < MAX_TELLERS; i++) {
        int val = semctl(sem_id, i, GETVAL);
        if (val == -1) {
            printf("semctl GETVAL failed for semaphore %d with errno = %d\n", i, errno);
            perror("semctl GETVAL failed");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore %d initialized with value: %d\n", i, val);
    }
}



void wait_semaphore(int sem_num) {
    printf("sem_num = %d\n", sem_num);
    struct sembuf sb = {sem_num, -1, 0};  // Semaphore'u bekle (decrement)
    printf("Waiting for semaphore %d...\n", sem_num);

    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop wait failed");
        exit(EXIT_FAILURE);
    }
    printf("Semaphore %d acquired.\n", sem_num);
}

void signal_semaphore(int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};  // Semaphore'u serbest bırak (increment)
    printf("Releasing semaphore %d...\n", sem_num);

    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop signal failed");
        exit(EXIT_FAILURE);
    }
    printf("Semaphore %d released.\n", sem_num);
}




pid_t Teller(void* func, void* arg_func) {
    printf("Creating Teller process\n");
    pid_t pid = fork();
    if (pid == 0) {
        printf("[Child] Teller process created, executing function.\n");
        ((void(*)(void*))func)(arg_func);
        exit(0);  // Çocuk işlem tamamlandıktan sonra çık
    } else if (pid > 0) {
        printf("[Parent] Teller process PID: %d\n", pid);
        // Ana süreçte çocuk sürecin PID'sini döndür
        return pid;
    } else {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
}


int waitTeller(pid_t pid, int* status) {
    return waitpid(pid, status, 0);  // Çocuk süreci bekle
}
void deposit(void* arg) {
    shared_data* data = (shared_data*)arg;
    int teller_num = data->account_id % MAX_TELLERS;  // Semafor numarasını hesapla

    // Semaphore ile shared memory'ye erişim bekle
    wait_semaphore(teller_num);

    // İşlemi yap (örneğin deposit işlemi)
    printf("[Teller] Processing deposit for BankID_%d, Amount: %d\n", data->account_id, data->amount);
    data->result = 0;  // Başarı ile işlem tamamlandı

    // Semaphore ile işlemi bitir
    signal_semaphore(teller_num);
}

void withdraw(void* arg) {
    shared_data* data = (shared_data*)arg;
    int teller_num = data->account_id % MAX_TELLERS;  // Semafor numarasını hesapla

    // Semaphore ile shared memory'ye erişim bekle
    wait_semaphore(teller_num);

    // İşlemi yap (örneğin withdraw işlemi)
    printf("[Teller] Processing withdrawal for BankID_%d, Amount: %d\n", data->account_id, data->amount);
    data->result = 0;  // Başarı ile işlem tamamlandı

    // Semaphore ile işlemi bitir
    signal_semaphore(teller_num);
}

typedef struct {
    char client_name[256];
    pid_t client_pid;
} ClientInfo;


ClientInfo batch_clients[MAX_BATCH];
pid_t teller_pids[MAX_TELLERS]; 
int teller_count = 0;
int batch_count = 0;

void cleanup_and_exit(int signum) {
    (void)signum; 
    printf("\nSignal received, closing active Tellers\n");
    for (int i = 0; i < teller_count; i++) {
        kill(teller_pids[i], SIGTERM);
    }
    unlink(SERVER_FIFO_NAME);
    printf("Removing ServerFIFO… Updating log file…\n");
    printf("Adabank says \"Bye\"...\n");
    exit(0);
}

void create_server_fifo(const char* fifo_name) {
    if (access(fifo_name, F_OK) == 0) {
        unlink(fifo_name);
    }
    if (mkfifo(fifo_name, 0666) == -1) {
        perror("[Server] Failed to create Server FIFO");
        exit(EXIT_FAILURE);
    }
}

int process_operation(const char* client_name, pid_t client_pid, const char* operation) {
    if (!(strncmp(operation, "N deposit", 9) == 0 || strncmp(operation, "BankID_", 7) == 0)) {
        return -1;  // Geçersiz işlem
    }

    if (strncmp(operation, "N deposit", 9) == 0) {
        int amount;
        sscanf(operation + 9, "%d", &amount);
        int new_id = create_account(client_name, amount);

        // ✨ HER ZAMAN KAYDET
        add_client_mapping(client_name, client_pid, new_id);

        save_database_to_log("AdaBank.bankLog");
        printf("%s deposited %d credits... updating log\n", client_name, amount);
        return 0;
    } else if (strncmp(operation, "BankID_", 7) == 0) {
        int account_id;
        char action[64];
        int amount;
        if (sscanf(operation, "BankID_%d %s %d", &account_id, action, &amount) == 3) {

            // ✨ HER BANKID OPERATION'DA mapping güncelle
            add_client_mapping(client_name, client_pid, account_id);

            if (strcmp(action, "deposit") == 0) {
                deposit_to_account(account_id, amount);
                save_database_to_log("AdaBank.bankLog");
                printf("%s deposited %d credits... updating log\n", client_name, amount);
                return 0;
            } else if (strcmp(action, "withdraw") == 0) {
                if (withdraw_from_account(account_id, amount) == 0) {
                    BankAccount* acc = find_account(account_id);
                    if (acc && acc->balance == 0) {
                        remove_account(account_id);
                        if (client_name != NULL) {
                            printf("%s withdraws %d credit.. updating log...  Bye %s\n", client_name, amount, client_name);
                        } else {
                            printf("%s withdraws %d credit.. updating log\n", client_name, amount); 
                        }
                    } else { 
                        printf("%s withdraws %d credit.. updating log\n", client_name, amount); 
                    }
                    save_database_to_log("AdaBank.bankLog");
                    return 0;
                }
                 else {
                    printf("%s withdraws %d credit..operation not permitted.\n", client_name, amount);
                    return -1;
                }
            }
        }
    }
    return -1;
}


typedef struct {
    char client_name[256];
    int pipe_fd;
} TellerArgs;


void handle_client(void* arg) {
    TellerArgs* args = (TellerArgs*)arg;
    const char* client_name = args->client_name;
    int pipe_fd = args->pipe_fd;
    signal(SIGTERM, cleanup_and_exit);

    char client_write_fifo[256];
    char client_read_fifo[256];

    snprintf(client_write_fifo, sizeof(client_write_fifo), "%s_write", client_name);
    snprintf(client_read_fifo, sizeof(client_read_fifo), "%s_read", client_name);

    int client_fd_read = open(client_write_fifo, O_RDONLY);
    int client_fd_write = open(client_read_fifo, O_WRONLY);

    if (client_fd_read == -1 || client_fd_write == -1) {
        perror("[Teller] Failed to open client FIFOs");
        exit(EXIT_FAILURE);
    }

    printf("-- Teller PID %d is active serving %s...\n", getpid(), client_name);

    char operation[256];
    while (1) {
        ssize_t bytes_read = read(client_fd_read, operation, sizeof(operation) - 1);
        if (bytes_read <= 0) {
            break;
        }
        operation[bytes_read] = '\0';

        size_t len = strlen(operation);
        while (len > 0 && (operation[len-1] == '\n' || operation[len-1] == '\r' || operation[len-1] == ' ')) {
            operation[--len] = '\0';
        }
        if (strlen(operation) == 0) continue;

        if (strcmp(operation, "exit") == 0) {
            break;
        }
        /*** --- BURASI EN ÖNEMLİ DEĞİŞİKLİK: Teller burada VALIDATE edecek: --- ***/
        // Teller içi validate kontrolü (yeniden yazıyorum)
        int valid = 0;
        if (strncmp(operation, "N deposit", 9) == 0) {
            valid = 1; // Yeni hesap açma her zaman valid
        } else if (strncmp(operation, "BankID_", 7) == 0) {
            int account_id;
            char action[64];
            int amount;
            if (sscanf(operation, "BankID_%d %s %d", &account_id, action, &amount) == 3) {
                if (strcmp(action, "deposit") == 0 || strcmp(action, "withdraw") == 0) {
                    valid = 1; // withdraw da olsa, deposit de olsa, **format doğruysa** valid
                }
            }
        }

        if (valid) {
            char formatted_operation[512];
                snprintf(formatted_operation, sizeof(formatted_operation), "%s %d %s\n", client_name, getpid(), operation);
                write(pipe_fd, formatted_operation, strlen(formatted_operation));

            char reply[256];
            snprintf(reply, sizeof(reply), "Operation '%.100s' sent to server.", operation);
            write(client_fd_write, reply, strlen(reply));
        } else {
            // ✨ Eğer geçersizse server'a hiçbir şey yollama
            char reply[256];
            snprintf(reply, sizeof(reply), "Operation '%.100s' failed or not permitted.", operation);
            write(client_fd_write, reply, strlen(reply));
        }
    }

    close(client_fd_read);
    close(client_fd_write);
    exit(EXIT_SUCCESS);
}

int main() {
    TellerArgs teller_args[MAX_BATCH];
    printf("Adabank is active….\n");

    if (access("AdaBank.bankLog", F_OK) == 0) {
        printf("Loading bank database from log...\n");
        load_database_from_log("AdaBank.bankLog");
    } else {
        printf("No previous logs.. Creating the bank database\n");
        init_database();
    }
    create_shared_memory();
    create_semaphore();
    create_server_fifo(SERVER_FIFO_NAME);
    printf("Waiting for clients @%s…\n", SERVER_FIFO_NAME);

    signal(SIGINT, cleanup_and_exit);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("[Server] Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    int server_fifo_fd = open(SERVER_FIFO_NAME, O_RDWR);
    if (server_fifo_fd == -1) {
        perror("[Server] Failed to open Server FIFO");
        exit(EXIT_FAILURE);
    }

    fd_set read_fds;
    while (1) {
        batch_count = 0;
        int first_batch_pid = -1;

        while (1) {
            FD_ZERO(&read_fds);
            FD_SET(server_fifo_fd, &read_fds);

            struct timeval timeout = {1, 0}; // 1 saniye batch toplama
            int activity = select(server_fifo_fd + 1, &read_fds, NULL, NULL, &timeout);

            if (activity == -1) {
                perror("[Server] select error");
                break;
            }

            if (FD_ISSET(server_fifo_fd, &read_fds)) {
                char buffer[512];
                ssize_t bytes_read = read(server_fifo_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    char* line = strtok(buffer, "\n");
                    while (line) {
                        char client_name[256];
                        pid_t client_pid;
                        if (sscanf(line, "%s %d", client_name, &client_pid) == 2) {
                            strcpy(batch_clients[batch_count].client_name, client_name);
                            batch_clients[batch_count].client_pid = client_pid;
                            if (first_batch_pid == -1) {
                                first_batch_pid = client_pid;
                            }
                        
                            batch_count++;
                        }
                        
                        
                        line = strtok(NULL, "\n");
                    }
                }
            }

            if (activity == 0) break; // timeout batchi bitir
        }

        if (batch_count > 0) {
            printf("- Received %d clients from PID %d\n", batch_count, first_batch_pid);
            for (int i = 0; i < batch_count; i++) {
                strcpy(teller_args[i].client_name, batch_clients[i].client_name);
                teller_args[i].pipe_fd = pipefd[1];  // pipefd[1] yazıyoruz çünkü client'a yazmak için kullanacağız
            
                pid_t teller_pid = Teller(handle_client, &teller_args[i]);
                teller_pids[teller_count++] = teller_pid;
                int known_account_id = find_client_account_id(batch_clients[i].client_name, batch_clients[i].client_pid);
                if (known_account_id != -1) {
                    printf("Welcome back %s (BankID_%02d)\n", batch_clients[i].client_name, known_account_id);
                } else {
                    BankAccount* acc = find_account_by_client_name(batch_clients[i].client_name);
                    if (acc != NULL) {
                        add_client_mapping(batch_clients[i].client_name, batch_clients[i].client_pid, acc->account_id);
                        printf("Welcome back %s (BankID_%02d)\n", batch_clients[i].client_name, acc->account_id);
                    }
                }
                int status;
                waitTeller(teller_pid, &status);
            }
            
        }

        FD_ZERO(&read_fds);
        FD_SET(pipefd[0], &read_fds);

        struct timeval small_timeout = {0, 100000}; // 100ms

        if (select(pipefd[0] + 1, &read_fds, NULL, NULL, &small_timeout) > 0) {
            if (FD_ISSET(pipefd[0], &read_fds)) {
                char op_buffer[4096];
                ssize_t bytes_read = read(pipefd[0], op_buffer, sizeof(op_buffer) - 1);
                if (bytes_read > 0) {
                    op_buffer[bytes_read] = '\0';
                    char* token = strtok(op_buffer, "\n");
                    while (token != NULL) {
                        char client_name[256];
                        pid_t client_pid;
                        char operation[256];
                                            
                        if (sscanf(token, "%s %d %[^\n]", client_name, &client_pid, operation) == 3) {
                            usleep(200000); // 200ms bekle
                            process_operation(client_name, client_pid, operation);
                        }
                        token = strtok(NULL, "\n");  // !!! sıradaki operation
                    }
                }
            }
        }

        printf("Waiting for clients @%s…\n", SERVER_FIFO_NAME);
    }

    close(server_fifo_fd);
    unlink(SERVER_FIFO_NAME);
    return 0;
}
