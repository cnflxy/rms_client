#pragma once

#include <cstdint>

constexpr uint32_t PIPE_MESSAGE_MAGIC = 0x55AA;

struct pipe_msg {
	uint32_t magic = PIPE_MESSAGE_MAGIC;
	uint32_t size = 0;
};
