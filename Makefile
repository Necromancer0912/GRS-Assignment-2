# MT25041
CC=gcc
CFLAGS=-O2 -Wall -Wextra -pthread -D_GNU_SOURCE

COMMON=MT25041_Part_Common.c

all: MT25041_Part_A1_Server MT25041_Part_A1_Client MT25041_Part_A2_Server MT25041_Part_A2_Client MT25041_Part_A3_Server MT25041_Part_A3_Client

MT25041_Part_A1_Server: MT25041_Part_A1_Server.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

MT25041_Part_A1_Client: MT25041_Part_A1_Client.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

MT25041_Part_A2_Server: MT25041_Part_A2_Server.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

MT25041_Part_A2_Client: MT25041_Part_A2_Client.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

MT25041_Part_A3_Server: MT25041_Part_A3_Server.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

MT25041_Part_A3_Client: MT25041_Part_A3_Client.c $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f MT25041_Part_A1_Server MT25041_Part_A1_Client MT25041_Part_A2_Server MT25041_Part_A2_Client MT25041_Part_A3_Server MT25041_Part_A3_Client
