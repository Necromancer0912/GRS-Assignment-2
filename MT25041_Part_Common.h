#ifndef MT25041_PART_COMMON_H
#define MT25041_PART_COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/errqueue.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIELD_COUNT 8

enum run_mode
{
    MODE_THROUGHPUT = 0,
    MODE_LATENCY = 1
};

enum send_mode
{
    SEND_BASELINE = 0,
    SEND_SENDMSG = 1,
    SEND_ZEROCOPY = 2
};

typedef struct
{
    char *field_buffers[FIELD_COUNT];
    size_t field_sizes[FIELD_COUNT];
    size_t total_message_size;
} message_t;

int run_server(int argument_count, char **argument_values);
int run_client(int argument_count, char **argument_values, enum send_mode mode);

#endif
