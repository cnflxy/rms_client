#pragma once

#include <vector>
#include <mutex>

class buffer_queue {
public:
	buffer_queue(const size_t& block_size = 8192)
	{
		if (!block_size) m_block_size = 8192;
		else m_block_size = (block_size + 8191) / 8192 * 8192;
	}

	~buffer_queue()
	{
		if (m_queue.empty()) return;

		std::lock_guard<std::mutex> lg(m_queue_mutex);

		m_valid = m_total = 0;

		while (!m_queue.empty()) {
			auto buf = m_queue.back();
			m_queue.pop_back();
			delete[] buf->data_buffer;
			delete buf;
		}
	}

	bool get(const size_t& size, void* buffer)
	{
		if (m_total < size) return false;

		std::lock_guard<std::mutex> lg(m_queue_mutex);

		if (m_total < size) return false;

		m_total -= size;

		for (size_t fetched = 0; fetched != size;) {
			auto buf = m_queue.front();
			size_t copy_size;

			if (buf->data_size - buf->offset <= size - fetched) {
				copy_size = buf->data_size - buf->offset;
			} else {
				copy_size = size - fetched;
			}

			memcpy(reinterpret_cast<uint8_t*>(buffer) + fetched, reinterpret_cast<uint8_t*>(buf->data_buffer) + buf->offset, copy_size);
			fetched += copy_size;
			buf->offset += copy_size;

			if (buf->offset == buf->data_size) {
				buf->data_size = buf->offset = 0;
				if (m_valid > 1) {
					m_queue.erase(m_queue.cbegin());
					m_queue.emplace_back(buf);
				}
				--m_valid;
			}
		}

		return true;
	}

	bool peek(const size_t& size, void* buffer)
	{
		if (m_total < size) return false;

		std::lock_guard<std::mutex> lg(m_queue_mutex);

		if (m_total < size) return false;

		auto buf = m_queue.cbegin();

		for (size_t fetched = 0; fetched != size;) {
			size_t copy_size;

			if ((*buf)->data_size - (*buf)->offset <= size - fetched) {
				copy_size = (*buf)->data_size - (*buf)->offset;
			} else {
				copy_size = size - fetched;
			}

			memcpy(reinterpret_cast<uint8_t*>(buffer) + fetched, reinterpret_cast<uint8_t*>((*buf)->data_buffer) + (*buf)->offset, copy_size);
			fetched += copy_size;

			if ((*buf)->offset + copy_size == (*buf)->data_size) {
				++buf;
			}
		}

		return true;
	}

	bool push(const void* data, const size_t& size)
	{
		std::lock_guard<std::mutex> lg(m_queue_mutex);

		size_t offset = 0;

		auto old_valid = m_valid;
		size_t old_offset, old_data_size;

		if (m_valid < m_queue.size() || m_total % m_block_size) {
			if (!m_valid) old_valid = m_valid = 1;
			for (;;) {
				auto buf = m_queue[m_valid - 1];
				if (buf->offset && buf->buffer_size - buf->data_size <= size - offset) {
					memcpy(buf->data_buffer, reinterpret_cast<uint8_t*>(buf->data_buffer) + buf->offset, buf->data_size - buf->offset);
					buf->data_size -= buf->offset;
					buf->offset = 0;
				}

				if (old_valid == m_valid) {
					old_offset = buf->offset;
					old_data_size = buf->data_size;
				}

				auto copy_size = std::min<>(size - offset, buf->buffer_size - buf->data_size);
				memcpy(reinterpret_cast<uint8_t*>(buf->data_buffer) + buf->data_size, reinterpret_cast<const uint8_t*>(data) + offset, copy_size);
				buf->data_size += copy_size;
				offset += copy_size;
				if (buf->data_size == buf->buffer_size && m_valid < m_queue.size()) {
					++m_valid;
				} else break;
			}
		}

		if (size != offset) {
			auto buf_size = (size - offset + m_block_size - 1) / m_block_size * m_block_size;

			auto buf = buffer::create(reinterpret_cast<const uint8_t*>(data) + offset, size - offset, buf_size);
			if (!buf) {
				if (offset) {
					m_valid = old_valid;
					buf = m_queue[m_valid - 1];
					buf->offset = old_offset;
					buf->data_size = old_data_size;
				}
				return false;
			}

			m_queue.emplace_back(buf);
			++m_valid;
		}

		m_total += size;

		return true;
	}

	size_t get_total() const
	{
		return m_total;
	}

private:
	struct buffer {
		size_t buffer_size;
		size_t data_size;
		size_t offset;
		void* data_buffer;

		static buffer* create(const void* data, const size_t& data_size, const size_t& buf_size)
		{
			void* data_copy = new (std::nothrow) uint8_t[buf_size];
			if (!data_copy) {
				return nullptr;
			}

			auto buf = new (std::nothrow) buffer(data_copy, data_size, buf_size);
			if (!buf) {
				delete[] data_copy;
				return nullptr;
			}

			memcpy(data_copy, data, data_size);

			return buf;
		}

		buffer(buffer&& rval) noexcept
		{
			move_from(std::forward<buffer>(std::move(rval)));
		}

		buffer(buffer&) = delete;
		buffer& operator=(const buffer&) = delete;

		buffer& operator=(buffer&& rval) noexcept
		{
			move_from(std::forward<buffer>(std::move(rval)));
		}

	private:
		buffer(void* data, const size_t& data_size, const size_t& buf_size) : data_buffer(data), data_size(data_size), buffer_size(buf_size), offset() {}

		void move_from(buffer&& rval)
		{
			data_buffer = rval.data_buffer;
			offset = rval.offset;
			data_size = rval.data_size;
			buffer_size = rval.buffer_size;

			rval.data_buffer = nullptr;
			rval.offset = rval.data_size = rval.buffer_size = 0;
		}
	};

	size_t m_block_size;
	std::vector<buffer*> m_queue;
	size_t m_valid = 0;
	size_t m_total = 0;
	std::mutex m_queue_mutex;
};
