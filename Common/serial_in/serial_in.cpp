#include "serial_in.hpp"

char serial_in_buffer[serial_in_buffer_size] = {};
u32  serial_in_buffer_next                   = 0;