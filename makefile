CC = gcc
CFLAGS = -Wall -Wextra -g
TARGETS = BankServer BankClient

all: $(TARGETS)

BankServer: bank_server.c bank_database.c
	$(CC) $(CFLAGS) -o BankServer bank_server.c bank_database.c -lrt

BankClient: bank_client.c
	$(CC) $(CFLAGS) -o BankClient bank_client.c

clean:
	rm -f $(TARGETS) *.o AdaBank.bankLog /tmp/*_fifo

