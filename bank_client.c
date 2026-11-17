#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#define MAX_FIFO_NAME_LEN 256
#define SERVER_FIFO "ServerFIFO"
#define MAX_CLIENTS 100

void safe_snprintf(char* buffer, size_t buffer_size, const char* format, const char* client_name, const char* suffix) {
    snprintf(buffer, buffer_size, format, client_name, suffix);
    buffer[buffer_size - 1] = '\0';
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ClientFile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* client_file_name = argv[1];
    FILE* client_file = fopen(client_file_name, "r");
    if (!client_file) {
        perror("[Client] Failed to open client file");
        exit(EXIT_FAILURE);
    }

    printf("[Client] Reading client file: %s\n", client_file_name);

    // 1. Her satırı oku, client adlarını ve işlemleri topla
    char operations[MAX_CLIENTS][256];
    int op_count = 0;

    while (fgets(operations[op_count], sizeof(operations[op_count]), client_file)) {
        operations[op_count][strcspn(operations[op_count], "\n")] = 0; // Remove newline
        op_count++;
    }
    fclose(client_file);

    // 2. Tüm clientların FIFO'larını oluştur
    for (int i = 0; i < op_count; i++) {
        char client_name[256];
        snprintf(client_name, sizeof(client_name), "Client%02d", i + 1);

        char write_fifo[MAX_FIFO_NAME_LEN], read_fifo[MAX_FIFO_NAME_LEN];
        safe_snprintf(write_fifo, sizeof(write_fifo), "%s_%s", client_name, "write");
        safe_snprintf(read_fifo, sizeof(read_fifo), "%s_%s", client_name, "read");

        mkfifo(write_fifo, 0666);
        mkfifo(read_fifo, 0666);
    }

    // 3. Bütün clientları ServerFIFO'ya bildir
    int server_fd = open(SERVER_FIFO, O_WRONLY);
    if (server_fd == -1) {
        perror("[Client] Failed to open Server FIFO");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < op_count; i++) {
        char client_name[256];
        snprintf(client_name, sizeof(client_name), "Client%02d", i + 1);

        char message[512];
        snprintf(message, sizeof(message), "%s %d\n", client_name, getpid());
        write(server_fd, message, strlen(message));
    }
    close(server_fd);

    // 4. FIFO'lardan işlemleri gönder
    usleep(500000); // Server teller açsın diye bekleme

    for (int i = 0; i < op_count; i++) {
        char client_name[256];
        snprintf(client_name, sizeof(client_name), "Client%02d", i + 1);

        char write_fifo[MAX_FIFO_NAME_LEN], read_fifo[MAX_FIFO_NAME_LEN];
        safe_snprintf(write_fifo, sizeof(write_fifo), "%s_%s", client_name, "write");
        safe_snprintf(read_fifo, sizeof(read_fifo), "%s_%s", client_name, "read");

        int write_fd = open(write_fifo, O_WRONLY);
        int read_fd = open(read_fifo, O_RDONLY);

        if (write_fd == -1 || read_fd == -1) {
            perror("[Client] Failed to open client FIFOs");
            exit(EXIT_FAILURE);
        }
        // FIFO'ları açtıktan hemen sonra ŞUNU EKLE:
if (strncmp(operations[i], "N deposit", 9) == 0) {
    int amount;
    sscanf(operations[i] + 9, "%d", &amount);
    printf("Connected..depositing %d credits\n", amount);
} else if (strncmp(operations[i], "BankID_", 7) == 0) {
    int id, amount;
    char action[64];
    if (sscanf(operations[i], "BankID_%d %s %d", &id, action, &amount) == 3) {
        if (strcmp(action, "deposit") == 0) {
            printf("Connected..depositing %d credits\n", amount);
        } else if (strcmp(action, "withdraw") == 0) {
            printf("Connected..withdrawing %d credits\n", amount);
        }
    }
}



        write(write_fd, operations[i], strlen(operations[i]));
        write(write_fd, "\n", 1);
        usleep(100000);

        write(write_fd, "exit\n", 5);
        usleep(100000);

        char response[256];
        ssize_t n = read(read_fd, response, sizeof(response) - 1);
        if (n > 0) {
            response[n] = '\0';
            printf("[Client] Server response: %s\n", response);
        }

        close(write_fd);
        close(read_fd);

        unlink(write_fifo);
        unlink(read_fifo);

        usleep(200000); // Biraz bekle
    }

    printf("[Client] All client operations done.\n");

    return 0;
}