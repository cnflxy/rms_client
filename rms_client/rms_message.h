#pragma once

#include <queue>
#include "utility.h"
#include "buffer_queue.h"

#pragma pack(push, 1)
struct rms_msg {
	using rms_msg_item = std::pair<size_t, rms_msg*>;
	using rms_msg_list = std::vector<rms_msg_item>;

	static bool make_msg(const std::vector<uint8_t>& data, rms_msg_item& msg_item, const bool& compressed = false, const uint32_t& raw_size = 0)
	{
		return make_msg(data.data(), data.size(), msg_item, compressed, raw_size);
	}

	static bool make_msg(const void* data, const size_t& size, rms_msg_item& msg_item, const bool& compressed = false, const uint32_t& raw_size = 0)
	{
		constexpr size_t msg_header_size = sizeof(rms_msg);

		auto msg_buf = new (std::nothrow) uint8_t[msg_header_size + size];
		if (!msg_buf) {
			return false;
		}

		GUID guid;
		utility::generate_uuid(guid);

		auto msg = new(msg_buf) rms_msg(size, guid, 0, 0, compressed, raw_size);
		msg->crc = utility::crc32(msg_buf, FIELD_OFFSET(rms_msg, crc));
		memcpy(msg_buf + msg_header_size, data, size);
		msg_item.first = msg_header_size + size;
		msg_item.second = msg;

		return true;
	}

	static bool make_msg_list(const std::vector<uint8_t>& data, rms_msg_list& msg_list, const bool& compressed = false, const uint32_t& raw_size = 0)
	{
		constexpr size_t msg_header_size = sizeof(rms_msg);
		GUID guid;
		size_t msg_size;
		size_t msg_data_size;
		const size_t max_data_size = MSG_LIST_ITEM_MAX_SIZE - msg_header_size;
		size_t cnt = data.size() / max_data_size, seq = 0;
		size_t fetched_size = 0, remain_size = data.size();

		utility::generate_uuid(guid);

		while (seq <= cnt) {
			if (remain_size < max_data_size) {
				msg_size = msg_header_size + remain_size;
				msg_data_size = remain_size;
			} else {
				msg_size = MSG_LIST_ITEM_MAX_SIZE;
				msg_data_size = max_data_size;
				remain_size -= msg_data_size;
			}

			auto msg_buf = new (std::nothrow) uint8_t[msg_size];
			if (!msg_buf) {
				free_msg_list(msg_list);
				return false;
			}

			auto msg = new(msg_buf) rms_msg(msg_data_size, guid, cnt, seq++, compressed, raw_size);
			msg->crc = utility::crc32(msg_buf, FIELD_OFFSET(rms_msg, crc));
			memcpy(msg_buf + msg_header_size, data.data() + fetched_size, msg_data_size);
			fetched_size += msg_data_size;
			msg_list.emplace_back(msg_size, msg);
		}

		return true;
	}

	static void free_msg(rms_msg_item& msg_item)
	{
		if (msg_item.first && msg_item.second) {
			delete[] msg_item.second;
		}
		msg_item.first = 0;
		msg_item.second = nullptr;
	}

	static void free_msg_list(rms_msg_list& msg_list)
	{
		std::for_each(msg_list.begin(), msg_list.end(),
			[](auto& item) {
				free_msg(item);
			}
		);
		msg_list.clear();
	}

	static bool probe_msg_header(buffer_queue* buf, rms_msg* msg_hdr)
	{
		constexpr size_t msg_header_size = sizeof(rms_msg);

		rms_msg msg;

		while (buf->get_total() >= msg_header_size) {
			buf->get(sizeof(msg.magic), &msg.magic);
			if (msg.magic != MSG_MAGIC) continue;
			buf->peek(msg_header_size - sizeof(msg.magic), &msg.guid);
			if (msg.crc != utility::crc32(&msg, FIELD_OFFSET(rms_msg, crc))) continue;
			buf->get(msg_header_size - sizeof(msg.magic), &msg_hdr->guid);
			return true;
		}

		return false;
	}

	rms_msg() = default;

	static constexpr size_t MSG_LIST_ITEM_MAX_SIZE = 1024 * 1024;
	static constexpr uint16_t MSG_MAGIC = 0x55AA;

	uint16_t magic = MSG_MAGIC;
	GUID guid{};
	uint8_t cnt = 0; // zero-based
	uint8_t seq = 0; // zero-based
#pragma warning(push)
#pragma warning(disable:4201)
	union {
		uint8_t flags = 0;
		struct {
			uint8_t compressed : 1;
			uint8_t reserved : 7;
		};
	};
#pragma warning(pop)
	uint32_t data_size = 0;
	uint32_t raw_size = 0;
	uint32_t crc = 0;
	// if data_size!=0 then uint8_t data[];

private:
	rms_msg(const uint32_t& _data_size, const GUID& _guid, const uint8_t& _cnt = 0, const uint8_t& _seq = 0, const bool& _compressed = false, const uint32_t& _raw_size = 0) : data_size(_data_size), guid(_guid), cnt(_cnt), seq(_seq), compressed(_compressed), raw_size(_raw_size)
	{
	}
};
#pragma pack(pop)
