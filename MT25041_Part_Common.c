#include "MT25041_Part_Common.h"

typedef struct
{
    char bind_ip_address[64];
    int port_number;
    size_t message_size;
    int maximum_clients;
    int enable_echo;
    int cpu_pin_base;
} server_config_t;

typedef struct
{
    char hostname[64];
    int port_number;
    size_t message_size;
    int thread_count;
    int duration_seconds;
    enum run_mode operation_mode;
    int enable_echo;
    int cpu_pin_base;
    int zerocopy_inflight_limit;
} client_config_t;

typedef struct
{
    int socket_file_descriptor;
    size_t message_size;
    int enable_echo;
    int thread_index;
    int cpu_pin_base;
} server_thread_context_t;

typedef struct
{
    int thread_index;
    int socket_file_descriptor;
    size_t message_size;
    int duration_seconds;
    enum run_mode operation_mode;
    int enable_echo;
    enum send_mode send_operation_mode;
    int cpu_pin_base;
    int zerocopy_inflight_limit;
    int zerocopy_enabled;
    uint64_t total_bytes_sent;
    uint64_t message_count;
    uint64_t round_trip_time_nanoseconds_sum;
    uint64_t elapsed_nanoseconds;
} client_thread_context_t;

static uint64_t now_ns(void)
{
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return (uint64_t)timestamp.tv_sec * 1000000000ULL + (uint64_t)timestamp.tv_nsec;
}

static size_t parse_size(const char *size_string)
{
    char *end_pointer = NULL;
    unsigned long long parsed_value = strtoull(size_string, &end_pointer, 10);
    if (end_pointer && *end_pointer)
    {
        char suffix_char = *end_pointer;
        if (suffix_char == 'k' || suffix_char == 'K')
        {
            parsed_value *= 1024ULL;
        }
        else if (suffix_char == 'm' || suffix_char == 'M')
        {
            parsed_value *= 1024ULL * 1024ULL;
        }
        else if (suffix_char == 'g' || suffix_char == 'G')
        {
            parsed_value *= 1024ULL * 1024ULL * 1024ULL;
        }
    }
    return (size_t)parsed_value;
}

static int pin_thread(int cpu_core_id)
{
    if (cpu_core_id < 0)
    {
        return 0;
    }
    cpu_set_t cpu_affinity_set;
    CPU_ZERO(&cpu_affinity_set);
    CPU_SET(cpu_core_id, &cpu_affinity_set);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_affinity_set), &cpu_affinity_set);
}

static int set_common_sockopts(int socket_file_descriptor)
{
    int enable_option = 1;
    if (setsockopt(socket_file_descriptor, IPPROTO_TCP, TCP_NODELAY, &enable_option, sizeof(enable_option)) != 0)
    {
        return -1;
    }
    return 0;
}

static int read_full(int socket_file_descriptor, void *buffer, size_t buffer_length)
{
    size_t bytes_read_offset = 0;
    while (bytes_read_offset < buffer_length)
    {
        ssize_t receive_result = recv(socket_file_descriptor, (char *)buffer + bytes_read_offset, buffer_length - bytes_read_offset, 0);
        if (receive_result == 0)
        {
            return 0;
        }
        if (receive_result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (receive_result == 0)
        {
            return -1;
        }
        bytes_read_offset += (size_t)receive_result;
    }
    return 1;
}

static int write_full(int socket_file_descriptor, const void *buffer, size_t buffer_length)
{
    size_t bytes_written_offset = 0;
    while (bytes_written_offset < buffer_length)
    {
        ssize_t send_result = send(socket_file_descriptor, (const char *)buffer + bytes_written_offset, buffer_length - bytes_written_offset, MSG_NOSIGNAL);
        if (send_result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        bytes_written_offset += (size_t)send_result;
    }
    return 1;
}

static void message_init(message_t *message_ptr, size_t total_size)
{
    size_t base_field_size = total_size / FIELD_COUNT;
    size_t size_remainder = total_size % FIELD_COUNT;
    message_ptr->total_message_size = total_size;
    for (int field_index = 0; field_index < FIELD_COUNT; field_index++)
    {
        size_t current_field_size = base_field_size + ((field_index == FIELD_COUNT - 1) ? size_remainder : 0);
        message_ptr->field_sizes[field_index] = current_field_size;
        message_ptr->field_buffers[field_index] = (char *)malloc(current_field_size);
        if (message_ptr->field_buffers[field_index])
        {
            memset(message_ptr->field_buffers[field_index], 'a' + field_index, current_field_size);
        }
    }
}

static void message_free(message_t *message_ptr)
{
    for (int field_index = 0; field_index < FIELD_COUNT; field_index++)
    {
        free(message_ptr->field_buffers[field_index]);
        message_ptr->field_buffers[field_index] = NULL;
        message_ptr->field_sizes[field_index] = 0;
    }
    message_ptr->total_message_size = 0;
}

static void message_pack(const message_t *message_ptr, char *destination_buffer)
{
    size_t buffer_offset = 0;
    for (int field_index = 0; field_index < FIELD_COUNT; field_index++)
    {
        memcpy(destination_buffer + buffer_offset, message_ptr->field_buffers[field_index], message_ptr->field_sizes[field_index]);
        buffer_offset += message_ptr->field_sizes[field_index];
    }
}

static void message_iov(const message_t *message_ptr, struct iovec *io_vector_array)
{
    for (int field_index = 0; field_index < FIELD_COUNT; field_index++)
    {
        io_vector_array[field_index].iov_base = message_ptr->field_buffers[field_index];
        io_vector_array[field_index].iov_len = message_ptr->field_sizes[field_index];
    }
}

static int sendmsg_full(int socket_file_descriptor, struct iovec *io_vector_array, int iovec_count, int send_flags)
{
    size_t total_bytes_to_send = 0;
    for (int vector_index = 0; vector_index < iovec_count; vector_index++)
    {
        total_bytes_to_send += io_vector_array[vector_index].iov_len;
    }
    size_t bytes_sent_total = 0;
    while (bytes_sent_total < total_bytes_to_send)
    {
        struct msghdr message_header;
        memset(&message_header, 0, sizeof(message_header));
        message_header.msg_iov = io_vector_array;
        message_header.msg_iovlen = iovec_count;
        ssize_t send_result = sendmsg(socket_file_descriptor, &message_header, send_flags);
        if (send_result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        bytes_sent_total += (size_t)send_result;
        size_t bytes_remaining = (size_t)send_result;
        int current_vector_index = 0;
        while (current_vector_index < iovec_count && bytes_remaining > 0)
        {
            if (bytes_remaining >= io_vector_array[current_vector_index].iov_len)
            {
                bytes_remaining -= io_vector_array[current_vector_index].iov_len;
                io_vector_array[current_vector_index].iov_len = 0;
                current_vector_index++;
            }
            else
            {
                io_vector_array[current_vector_index].iov_base = (char *)io_vector_array[current_vector_index].iov_base + bytes_remaining;
                io_vector_array[current_vector_index].iov_len -= bytes_remaining;
                bytes_remaining = 0;
            }
        }
        while (iovec_count > 0 && io_vector_array[0].iov_len == 0)
        {
            io_vector_array++;
            iovec_count--;
        }
    }
    return 1;
}

static int create_server_socket(const char *bind_ip_address, int port_number)
{
    int listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_fd < 0)
    {
        return -1;
    }
    int enable_reuse_option = 1;
    setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse_option, sizeof(enable_reuse_option));
    set_common_sockopts(listen_socket_fd);

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((uint16_t)port_number);
    server_address.sin_addr.s_addr = bind_ip_address[0] ? inet_addr(bind_ip_address) : INADDR_ANY;

    if (bind(listen_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0)
    {
        close(listen_socket_fd);
        return -1;
    }
    if (listen(listen_socket_fd, 128) != 0)
    {
        close(listen_socket_fd);
        return -1;
    }
    return listen_socket_fd;
}

static int create_client_socket(const char *hostname, int port_number)
{
    int client_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket_fd < 0)
    {
        return -1;
    }
    set_common_sockopts(client_socket_fd);

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((uint16_t)port_number);
    if (inet_pton(AF_INET, hostname, &server_address.sin_addr) != 1)
    {
        close(client_socket_fd);
        return -1;
    }
    if (connect(client_socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0)
    {
        close(client_socket_fd);
        return -1;
    }
    return client_socket_fd;
}

static void *server_thread_main(void *thread_argument)
{
    server_thread_context_t *thread_context = (server_thread_context_t *)thread_argument;
    if (thread_context->cpu_pin_base >= 0)
    {
        pin_thread(thread_context->cpu_pin_base + thread_context->thread_index);
    }
    char *receive_buffer = (char *)malloc(thread_context->message_size);
    if (!receive_buffer)
    {
        close(thread_context->socket_file_descriptor);
        return NULL;
    }

    while (1)
    {
        int read_result = read_full(thread_context->socket_file_descriptor, receive_buffer, thread_context->message_size);
        if (read_result <= 0)
        {
            break;
        }
        if (thread_context->enable_echo)
        {
            if (write_full(thread_context->socket_file_descriptor, receive_buffer, thread_context->message_size) <= 0)
            {
                break;
            }
        }
    }

    free(receive_buffer);
    close(thread_context->socket_file_descriptor);
    return NULL;
}

static int parse_server_args(int argument_count, char **argument_values, server_config_t *server_config)
{
    server_config->bind_ip_address[0] = '\0';
    server_config->port_number = 5001;
    server_config->message_size = 1024;
    server_config->maximum_clients = 1;
    server_config->enable_echo = 0;
    server_config->cpu_pin_base = -1;

    for (int arg_index = 1; arg_index < argument_count; arg_index++)
    {
        if (strcmp(argument_values[arg_index], "--bind") == 0 && arg_index + 1 < argument_count)
        {
            snprintf(server_config->bind_ip_address, sizeof(server_config->bind_ip_address), "%s", argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--port") == 0 && arg_index + 1 < argument_count)
        {
            server_config->port_number = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--msg-size") == 0 && arg_index + 1 < argument_count)
        {
            server_config->message_size = parse_size(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--max-clients") == 0 && arg_index + 1 < argument_count)
        {
            server_config->maximum_clients = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--echo") == 0)
        {
            server_config->enable_echo = 1;
        }
        else if (strcmp(argument_values[arg_index], "--pin-base") == 0 && arg_index + 1 < argument_count)
        {
            server_config->cpu_pin_base = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--help") == 0)
        {
            return -1;
        }
    }
    return 0;
}

static void usage_server(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s [--bind ip] [--port p] [--msg-size n] [--max-clients n] [--echo] [--pin-base cpu]\n",
            program_name);
}

static int parse_client_args(int argument_count, char **argument_values, client_config_t *client_config)
{
    snprintf(client_config->hostname, sizeof(client_config->hostname), "127.0.0.1");
    client_config->port_number = 5001;
    client_config->message_size = 1024;
    client_config->thread_count = 1;
    client_config->duration_seconds = 5;
    client_config->operation_mode = MODE_THROUGHPUT;
    client_config->enable_echo = 0;
    client_config->cpu_pin_base = -1;
    client_config->zerocopy_inflight_limit = 32;

    for (int arg_index = 1; arg_index < argument_count; arg_index++)
    {
        if (strcmp(argument_values[arg_index], "--host") == 0 && arg_index + 1 < argument_count)
        {
            snprintf(client_config->hostname, sizeof(client_config->hostname), "%s", argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--port") == 0 && arg_index + 1 < argument_count)
        {
            client_config->port_number = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--msg-size") == 0 && arg_index + 1 < argument_count)
        {
            client_config->message_size = parse_size(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--threads") == 0 && arg_index + 1 < argument_count)
        {
            client_config->thread_count = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--duration") == 0 && arg_index + 1 < argument_count)
        {
            client_config->duration_seconds = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--mode") == 0 && arg_index + 1 < argument_count)
        {
            const char *mode_string = argument_values[++arg_index];
            if (strcmp(mode_string, "latency") == 0)
            {
                client_config->operation_mode = MODE_LATENCY;
            }
            else
            {
                client_config->operation_mode = MODE_THROUGHPUT;
            }
        }
        else if (strcmp(argument_values[arg_index], "--echo") == 0)
        {
            client_config->enable_echo = 1;
        }
        else if (strcmp(argument_values[arg_index], "--pin-base") == 0 && arg_index + 1 < argument_count)
        {
            client_config->cpu_pin_base = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--zc-inflight") == 0 && arg_index + 1 < argument_count)
        {
            client_config->zerocopy_inflight_limit = atoi(argument_values[++arg_index]);
        }
        else if (strcmp(argument_values[arg_index], "--help") == 0)
        {
            return -1;
        }
    }

    if (client_config->operation_mode == MODE_LATENCY)
    {
        client_config->enable_echo = 1;
    }
    if (client_config->zerocopy_inflight_limit < 1)
    {
        client_config->zerocopy_inflight_limit = 1;
    }
    return 0;
}

static void usage_client(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s [--host ip] [--port p] [--msg-size n] [--threads n] [--duration s] [--mode throughput|latency] [--echo] [--pin-base cpu] [--zc-inflight n]\n",
            program_name);
}

static int zerocopy_enable(int socket_file_descriptor)
{
#ifdef SO_ZEROCOPY
    int enable_option = 1;
    if (setsockopt(socket_file_descriptor, SOL_SOCKET, SO_ZEROCOPY, &enable_option, sizeof(enable_option)) == 0)
    {
        return 1;
    }
#endif
    return 0;
}

static int zerocopy_reap(int socket_file_descriptor, int blocking_mode, int *inflight_count_ptr)
{
    char control_message_buffer[256];
    char single_data_byte;
    struct iovec io_vector;
    struct msghdr message_header;

    memset(&message_header, 0, sizeof(message_header));
    io_vector.iov_base = &single_data_byte;
    io_vector.iov_len = sizeof(single_data_byte);
    message_header.msg_iov = &io_vector;
    message_header.msg_iovlen = 1;
    message_header.msg_control = control_message_buffer;
    message_header.msg_controllen = sizeof(control_message_buffer);

    int receive_flags = MSG_ERRQUEUE | (blocking_mode ? 0 : MSG_DONTWAIT);
    ssize_t receive_result = recvmsg(socket_file_descriptor, &message_header, receive_flags);
    if (receive_result < 0)
    {
        if (!blocking_mode && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return 0;
        }
        return -1;
    }

    for (struct cmsghdr *control_msg_ptr = CMSG_FIRSTHDR(&message_header); control_msg_ptr; control_msg_ptr = CMSG_NXTHDR(&message_header, control_msg_ptr))
    {
        if (control_msg_ptr->cmsg_level == SOL_IP && control_msg_ptr->cmsg_type == IP_RECVERR)
        {
            struct sock_extended_err *socket_error_ptr = (struct sock_extended_err *)CMSG_DATA(control_msg_ptr);
            if (socket_error_ptr && socket_error_ptr->ee_origin == SO_EE_ORIGIN_ZEROCOPY)
            {
                uint32_t completion_range_start = socket_error_ptr->ee_info;
                uint32_t completion_range_end = socket_error_ptr->ee_data;
                int completed_operations_count = 1;
                if (completion_range_end >= completion_range_start && completion_range_start != 0)
                {
                    completed_operations_count = (int)(completion_range_end - completion_range_start + 1);
                }
                *inflight_count_ptr -= completed_operations_count;
                if (*inflight_count_ptr < 0)
                {
                    *inflight_count_ptr = 0;
                }
            }
        }
    }
    return 1;
}

static void *client_thread_main(void *thread_argument)
{
    client_thread_context_t *thread_context = (client_thread_context_t *)thread_argument;
    if (thread_context->cpu_pin_base >= 0)
    {
        pin_thread(thread_context->cpu_pin_base + thread_context->thread_index);
    }

    int zerocopy_inflight_operations = 0;
    message_t *message_buffers_array = NULL;
    char *send_packed_buffer = NULL;
    char *receive_buffer = NULL;
    struct iovec io_vector_array[FIELD_COUNT];

    if (thread_context->send_operation_mode == SEND_BASELINE)
    {
        message_t current_message;
        message_init(&current_message, thread_context->message_size);
        send_packed_buffer = (char *)malloc(thread_context->message_size);
        if (!send_packed_buffer)
        {
            message_free(&current_message);
            return NULL;
        }
        uint64_t operation_start_time_ns = now_ns();
        while (now_ns() - operation_start_time_ns < (uint64_t)thread_context->duration_seconds * 1000000000ULL)
        {
            uint64_t message_send_start_time_ns = 0;
            if (thread_context->operation_mode == MODE_LATENCY)
            {
                message_send_start_time_ns = now_ns();
            }
            message_pack(&current_message, send_packed_buffer);
            if (write_full(thread_context->socket_file_descriptor, send_packed_buffer, thread_context->message_size) <= 0)
            {
                break;
            }
            thread_context->total_bytes_sent += thread_context->message_size;
            thread_context->message_count++;

            if (thread_context->enable_echo)
            {
                if (read_full(thread_context->socket_file_descriptor, send_packed_buffer, thread_context->message_size) <= 0)
                {
                    break;
                }
            }
            if (thread_context->operation_mode == MODE_LATENCY)
            {
                uint64_t message_send_end_time_ns = now_ns();
                thread_context->round_trip_time_nanoseconds_sum += (message_send_end_time_ns - message_send_start_time_ns);
            }
        }
        thread_context->elapsed_nanoseconds = now_ns() - operation_start_time_ns;
        message_free(&current_message);
        free(send_packed_buffer);
        return NULL;
    }

    int message_buffer_count = (thread_context->send_operation_mode == SEND_ZEROCOPY) ? thread_context->zerocopy_inflight_limit : 1;
    if (message_buffer_count < 1)
    {
        message_buffer_count = 1;
    }
    message_buffers_array = (message_t *)calloc((size_t)message_buffer_count, sizeof(message_t));
    for (int buffer_index = 0; buffer_index < message_buffer_count; buffer_index++)
    {
        message_init(&message_buffers_array[buffer_index], thread_context->message_size);
    }
    if (thread_context->enable_echo)
    {
        receive_buffer = (char *)malloc(thread_context->message_size);
        if (!receive_buffer)
        {
            for (int buffer_index = 0; buffer_index < message_buffer_count; buffer_index++)
            {
                message_free(&message_buffers_array[buffer_index]);
            }
            free(message_buffers_array);
            return NULL;
        }
    }

    if (thread_context->send_operation_mode == SEND_ZEROCOPY)
    {
        thread_context->zerocopy_enabled = zerocopy_enable(thread_context->socket_file_descriptor);
    }

    uint64_t operation_start_time_ns = now_ns();
    while (now_ns() - operation_start_time_ns < (uint64_t)thread_context->duration_seconds * 1000000000ULL)
    {
        int current_buffer_index = (int)(thread_context->message_count % (uint64_t)message_buffer_count);
        message_iov(&message_buffers_array[current_buffer_index], io_vector_array);

        uint64_t message_send_start_time_ns = 0;
        if (thread_context->operation_mode == MODE_LATENCY)
        {
            message_send_start_time_ns = now_ns();
        }

        int send_flags = MSG_NOSIGNAL;
        if (thread_context->send_operation_mode == SEND_ZEROCOPY && thread_context->zerocopy_enabled)
        {
#ifdef MSG_ZEROCOPY
            send_flags |= MSG_ZEROCOPY;
#endif
        }

        struct iovec io_vector_working_copy[FIELD_COUNT];
        memcpy(io_vector_working_copy, io_vector_array, sizeof(io_vector_working_copy));
        int send_result = sendmsg_full(thread_context->socket_file_descriptor, io_vector_working_copy, FIELD_COUNT, send_flags);
        if (send_result < 0)
        {
            if (thread_context->send_operation_mode == SEND_ZEROCOPY && thread_context->zerocopy_enabled &&
                (errno == EINVAL || errno == EOPNOTSUPP))
            {
                thread_context->zerocopy_enabled = 0;
                continue;
            }
            break;
        }
        thread_context->total_bytes_sent += thread_context->message_size;
        thread_context->message_count++;

        if (thread_context->send_operation_mode == SEND_ZEROCOPY && thread_context->zerocopy_enabled)
        {
            zerocopy_inflight_operations++;
            if (zerocopy_inflight_operations >= thread_context->zerocopy_inflight_limit)
            {
                while (zerocopy_inflight_operations >= thread_context->zerocopy_inflight_limit)
                {
                    if (zerocopy_reap(thread_context->socket_file_descriptor, 1, &zerocopy_inflight_operations) < 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                zerocopy_reap(thread_context->socket_file_descriptor, 0, &zerocopy_inflight_operations);
            }
        }

        if (thread_context->enable_echo)
        {
            if (read_full(thread_context->socket_file_descriptor, receive_buffer, thread_context->message_size) <= 0)
            {
                break;
            }
        }

        if (thread_context->operation_mode == MODE_LATENCY)
        {
            uint64_t message_send_end_time_ns = now_ns();
            thread_context->round_trip_time_nanoseconds_sum += (message_send_end_time_ns - message_send_start_time_ns);
        }
    }

    if (thread_context->send_operation_mode == SEND_ZEROCOPY && thread_context->zerocopy_enabled)
    {
        while (zerocopy_inflight_operations > 0)
        {
            if (zerocopy_reap(thread_context->socket_file_descriptor, 1, &zerocopy_inflight_operations) < 0)
            {
                break;
            }
        }
    }

    thread_context->elapsed_nanoseconds = now_ns() - operation_start_time_ns;

    for (int buffer_index = 0; buffer_index < message_buffer_count; buffer_index++)
    {
        message_free(&message_buffers_array[buffer_index]);
    }
    free(message_buffers_array);
    free(receive_buffer);
    return NULL;
}

int run_server(int argument_count, char **argument_values)
{
    server_config_t server_configuration;
    if (parse_server_args(argument_count, argument_values, &server_configuration) != 0)
    {
        usage_server(argument_values[0]);
        return 1;
    }

    int listen_socket_fd = create_server_socket(server_configuration.bind_ip_address, server_configuration.port_number);
    if (listen_socket_fd < 0)
    {
        perror("listen");
        return 1;
    }

    pthread_t *server_thread_array = (pthread_t *)calloc((size_t)server_configuration.maximum_clients, sizeof(pthread_t));
    server_thread_context_t *thread_context_array = (server_thread_context_t *)calloc((size_t)server_configuration.maximum_clients, sizeof(server_thread_context_t));
    if (!server_thread_array || !thread_context_array)
    {
        close(listen_socket_fd);
        return 1;
    }

    int accepted_connections_count = 0;
    while (accepted_connections_count < server_configuration.maximum_clients)
    {
        int accepted_socket_fd = accept(listen_socket_fd, NULL, NULL);
        if (accepted_socket_fd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        thread_context_array[accepted_connections_count].socket_file_descriptor = accepted_socket_fd;
        thread_context_array[accepted_connections_count].message_size = server_configuration.message_size;
        thread_context_array[accepted_connections_count].enable_echo = server_configuration.enable_echo;
        thread_context_array[accepted_connections_count].thread_index = accepted_connections_count;
        thread_context_array[accepted_connections_count].cpu_pin_base = server_configuration.cpu_pin_base;
        pthread_create(&server_thread_array[accepted_connections_count], NULL, server_thread_main, &thread_context_array[accepted_connections_count]);
        accepted_connections_count++;
    }

    close(listen_socket_fd);

    for (int thread_index = 0; thread_index < accepted_connections_count; thread_index++)
    {
        pthread_join(server_thread_array[thread_index], NULL);
    }

    free(server_thread_array);
    free(thread_context_array);
    return 0;
}

int run_client(int argument_count, char **argument_values, enum send_mode send_operation_mode)
{
    client_config_t client_configuration;
    if (parse_client_args(argument_count, argument_values, &client_configuration) != 0)
    {
        usage_client(argument_values[0]);
        return 1;
    }

    pthread_t *client_thread_array = (pthread_t *)calloc((size_t)client_configuration.thread_count, sizeof(pthread_t));
    client_thread_context_t *thread_context_array = (client_thread_context_t *)calloc((size_t)client_configuration.thread_count, sizeof(client_thread_context_t));
    if (!client_thread_array || !thread_context_array)
    {
        return 1;
    }

    for (int thread_index = 0; thread_index < client_configuration.thread_count; thread_index++)
    {
        int connection_socket_fd = create_client_socket(client_configuration.hostname, client_configuration.port_number);
        if (connection_socket_fd < 0)
        {
            fprintf(stderr, "connect failed\n");
            return 1;
        }
        thread_context_array[thread_index].thread_index = thread_index;
        thread_context_array[thread_index].socket_file_descriptor = connection_socket_fd;
        thread_context_array[thread_index].message_size = client_configuration.message_size;
        thread_context_array[thread_index].duration_seconds = client_configuration.duration_seconds;
        thread_context_array[thread_index].operation_mode = client_configuration.operation_mode;
        thread_context_array[thread_index].enable_echo = client_configuration.enable_echo;
        thread_context_array[thread_index].send_operation_mode = send_operation_mode;
        thread_context_array[thread_index].cpu_pin_base = client_configuration.cpu_pin_base;
        thread_context_array[thread_index].zerocopy_inflight_limit = client_configuration.zerocopy_inflight_limit;
        pthread_create(&client_thread_array[thread_index], NULL, client_thread_main, &thread_context_array[thread_index]);
    }

    uint64_t aggregated_total_bytes = 0;
    uint64_t aggregated_total_messages = 0;
    uint64_t aggregated_round_trip_time_ns = 0;
    uint64_t maximum_elapsed_nanoseconds = 0;

    for (int thread_index = 0; thread_index < client_configuration.thread_count; thread_index++)
    {
        pthread_join(client_thread_array[thread_index], NULL);
        close(thread_context_array[thread_index].socket_file_descriptor);
        aggregated_total_bytes += thread_context_array[thread_index].total_bytes_sent;
        aggregated_total_messages += thread_context_array[thread_index].message_count;
        aggregated_round_trip_time_ns += thread_context_array[thread_index].round_trip_time_nanoseconds_sum;
        if (thread_context_array[thread_index].elapsed_nanoseconds > maximum_elapsed_nanoseconds)
        {
            maximum_elapsed_nanoseconds = thread_context_array[thread_index].elapsed_nanoseconds;
        }
    }

    double elapsed_time_seconds = (maximum_elapsed_nanoseconds > 0) ? (double)maximum_elapsed_nanoseconds / 1e9 : 0.0;
    double calculated_throughput_gbps = 0.0;
    double calculated_latency_microseconds = 0.0;

    if (elapsed_time_seconds > 0.0)
    {
        calculated_throughput_gbps = ((double)aggregated_total_bytes * 8.0) / (elapsed_time_seconds * 1e9);
    }
    if (client_configuration.operation_mode == MODE_LATENCY && aggregated_total_messages > 0)
    {
        calculated_latency_microseconds = ((double)aggregated_round_trip_time_ns / (double)aggregated_total_messages) / 1000.0;
    }

    printf("RESULT,%.6f,%.3f,%llu,%.6f\n",
           calculated_throughput_gbps,
           calculated_latency_microseconds,
           (unsigned long long)aggregated_total_bytes,
           elapsed_time_seconds);

    free(client_thread_array);
    free(thread_context_array);
    return 0;
}
