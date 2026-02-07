#include "MT25041_Part_Common.h"

int main(int argument_count, char **argument_values)
{
    return run_client(argument_count, argument_values, SEND_ZEROCOPY);
}
