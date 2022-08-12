#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <bcrypt.h>
#include <zlib/zlib.h>
#include <zstd/zstd.h>

#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)

class zip {
	typedef
		NTSTATUS(WINAPI* PFN_RtlCompressBuffer)(
			_In_ USHORT CompressionFormatAndEngine,
			_In_  PUCHAR UncompressedBuffer,
			_In_  ULONG  UncompressedBufferSize,
			_Out_ PUCHAR CompressedBuffer,
			_In_  ULONG  CompressedBufferSize,
			_In_  ULONG  UncompressedChunkSize,
			_Out_ PULONG FinalCompressedSize,
			_In_  PVOID  WorkSpace
			);

	typedef
		NTSTATUS(WINAPI* PFN_RtlDecompressBuffer)(
			_In_  USHORT CompressionFormat,
			_Out_ PUCHAR UncompressedBuffer,
			_In_  ULONG  UncompressedBufferSize,
			_In_  PUCHAR CompressedBuffer,
			_In_  ULONG  CompressedBufferSize,
			_Out_ PULONG FinalUncompressedSize
			);

	typedef
		NTSTATUS(WINAPI* PFN_RtlGetCompressionWorkSpaceSize)(
			_In_  USHORT CompressionFormatAndEngine,
			_Out_ PULONG CompressBufferWorkSpaceSize,
			_Out_ PULONG CompressFragmentWorkSpaceSize
			);

public:
	struct compressd_data_type {
		size_t raw_size;
		std::vector<uint8_t> data;
	};

	static bool compress_native(const void* in, const size_t& len, compressd_data_type& out) {
		NTSTATUS Status;
		HMODULE hNtdll;
		PFN_RtlCompressBuffer CompressBuffer;
		PFN_RtlGetCompressionWorkSpaceSize GetCompressionWorkSpaceSize;
		ULONG cbCompressBufferSize, cbCompressFragmentSize, cbCompressedSize;
		PBYTE pbCompressBuffer = nullptr;

		//constexpr NTSTATUS STATUS_BUFFER_TOO_SMALL = 0xc0000023;
		//constexpr NTSTATUS STATUS_UNSUPPORTED_COMPRESSION = 0xC000025F;

		out.raw_size = 0;
		out.data.clear();
		if (!len) {
			goto exit;
		}

		hNtdll = GetModuleHandleW(L"ntdll");
		assert(hNtdll != nullptr);
		CompressBuffer = reinterpret_cast<PFN_RtlCompressBuffer>(GetProcAddress(hNtdll, "RtlCompressBuffer"));
		GetCompressionWorkSpaceSize = reinterpret_cast<PFN_RtlGetCompressionWorkSpaceSize>(GetProcAddress(hNtdll, "RtlGetCompressionWorkSpaceSize"));

		Status = GetCompressionWorkSpaceSize(
			COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_MAXIMUM,
			&cbCompressBufferSize,
			&cbCompressFragmentSize
		);
		if (!NT_SUCCESS(Status)) {
			std::cerr << "GetCompressionWorkSpaceSize failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		pbCompressBuffer = new (std::nothrow) BYTE[cbCompressBufferSize];
		if (!pbCompressBuffer) {
			std::cerr << "alloc CompressBuffer failed!" << std::endl;
			goto exit;
		}

		out.data.resize(len - 1);	// 设置成原始数据大小-1

		//COMPRESSION_FORMAT_XPRESS | COMPRESSION_ENGINE_MAXIMUM,  4448 - 1592
		//COMPRESSION_FORMAT_XPRESS_HUFF | COMPRESSION_ENGINE_MAXIMUM,  4448 - 1492
		//COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_MAXIMUM,  4448 - 1629
		Status = CompressBuffer(
			COMPRESSION_FORMAT_LZNT1 | COMPRESSION_ENGINE_MAXIMUM,
			reinterpret_cast<PUCHAR>(const_cast<void*>(in)),
			len,
			out.data.data(),
			out.data.size(),
			4096,
			&cbCompressedSize,
			pbCompressBuffer
		);

		delete[] pbCompressBuffer;

		if (!NT_SUCCESS(Status)) {	// 压缩后的大小可能大于原始数据大小，但是这不是我们压缩的目的
			out.data.clear();
			std::cerr << "CompressBuffer failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}

		out.raw_size = len;
		out.data.resize(cbCompressedSize);
		out.data.shrink_to_fit();

		std::cout << "in: " << len << " out: " << cbCompressedSize << std::endl;

	exit:

		return !out.data.empty();
	};

	static bool decompress_native(const compressd_data_type& in, std::vector<uint8_t>& out) {
		NTSTATUS Status;
		HMODULE hNtdll;
		PFN_RtlDecompressBuffer DecompressBuffer;
		ULONG cbDecompressedSize;

		//constexpr NTSTATUS STATUS_BUFFER_TOO_SMALL = 0xc0000023;
		//constexpr NTSTATUS STATUS_UNSUPPORTED_COMPRESSION = 0xC000025F;
		//constexpr NTSTATUS STATUS_BAD_COMPRESSION_BUFFER = 0xC0000242;

		out.clear();
		if (in.data.empty() || in.raw_size < in.data.size()) {
			goto exit;
		}

		hNtdll = GetModuleHandleW(L"ntdll");
		assert(hNtdll != nullptr);
		DecompressBuffer = reinterpret_cast<PFN_RtlDecompressBuffer>(GetProcAddress(hNtdll, "RtlDecompressBuffer"));

		out.resize(in.raw_size);

		// STATUS_INVALID_USER_BUFFER
		Status = DecompressBuffer(
			COMPRESSION_FORMAT_LZNT1,
			out.data(),
			out.size(),
			const_cast<PUCHAR>(in.data.data()),
			in.data.size(),
			&cbDecompressedSize
		);
		if (!NT_SUCCESS(Status)) {
			out.clear();
			std::cerr << "DecompressBuffer failed: " << std::hex << std::showbase << Status << std::noshowbase << std::dec << std::endl;
			goto exit;
		}
		if (cbDecompressedSize != out.size()) {
			out.clear();
			goto exit;
		}

		std::cout << "in: " << in.data.size() << " out: " << cbDecompressedSize << std::endl;

	exit:

		return !out.empty();
	};

	static bool compress_zlib(const void* in, const size_t& len, compressd_data_type& out) {
		//return false;
		const auto& max_out_size = compressBound(len);
		uLongf out_size;

		out.data.clear();
		out.raw_size = 0;
		if (!len) return false;

		out.data.resize(len - 1);

		int status = compress2(out.data.data(), &out_size, reinterpret_cast<const Bytef*>(in), len, 6);
		if (Z_OK == status && out_size < len) {
			out.data.resize(out_size);
			out.data.shrink_to_fit();
			out.raw_size = len;

			std::cout << "in: " << len << " out: " << out_size << " max_out_size: " << max_out_size << std::endl;
		} else {
			out.data.clear();
		}

		return !out.data.empty();
	};

	static bool decompress_zlib(const compressd_data_type& in, std::vector<uint8_t>& out) {
		uLongf out_size, consumed_size;

		out.clear();
		if (in.data.empty() || in.raw_size < in.data.size()) return false;

		out.resize(out_size = in.raw_size);

		int status = uncompress2(out.data(), &out_size, in.data.data(), &consumed_size);
		if (Z_OK == status) {
			if (out_size != in.raw_size || consumed_size != in.data.size()) {
				out.clear();
			}

			std::cout << "in: " << in.data.size() << " out: " << out_size << " consumed: " << consumed_size << std::endl;
		} else {
			out.clear();
		}

		return !out.empty();
	};

	static bool decompress_zlib(const void* in, const size_t& size, const size_t& raw_size, std::vector<uint8_t>& out) {
		uLongf out_size, consumed_size;

		out.clear();
		if (!size || raw_size < size) return false;

		out.resize(out_size = raw_size);

		int status = uncompress2(out.data(), &out_size, (const Bytef*)in, &consumed_size);
		if (Z_OK == status) {
			if (out_size != raw_size || consumed_size != size) {
				out.clear();
			}

			std::cout << "in: " << size << " expect: " << out_size << " out: " << out_size << " consumed: " << consumed_size << std::endl;
		} else {
			out.clear();
		}

		return !out.empty();
	};

	static bool compress_zstd(const void* in, const size_t& len, compressd_data_type& out) {
		const auto& max_out_size = ZSTD_compressBound(len);
		const auto max_level = ZSTD_maxCLevel();
		size_t out_size;

		out.data.clear();
		out.raw_size = 0;
		if (!len) return false;

		out.data.resize(max_out_size);

		out_size = ZSTD_compress(out.data.data(), out.data.size(), in, len, max_level);
		if (!ZSTD_isError(out_size)) {
			if (out_size < len) {
				out.data.resize(out_size);
				out.data.shrink_to_fit();
				out.raw_size = len;
			} else {
				out.data.clear();
			}

			std::cout << "in: " << len << " out: " << out_size << " max_out_size: " << max_out_size << std::endl;
		} else {
			out.data.clear();
		}

		return !out.data.empty();
	};

	static bool decompress_zstd(const void* in, const size_t& size, const size_t& raw_size, std::vector<uint8_t>& out) {
		size_t out_size, decompressed_size;

		out.clear();
		if (!size || raw_size < size) return false;

		//out_size = ZSTD_getFrameContentSize(in.data.data(), in.data.size());
		//if (out_size <= 0) out_size = in.raw_size;
		//if (out_size == ZSTD_CONTENTSIZE_ERROR) return false;
		//else if (out_size == ZSTD_CONTENTSIZE_UNKNOWN) out_size = in.size() * 5;

		out.resize(out_size = raw_size);

		decompressed_size = ZSTD_decompress(out.data(), out.size(), in, size);
		if (!ZSTD_isError(decompressed_size)) {
			if (decompressed_size != raw_size) {
				out.clear();
			}

			std::cout << "in: " << size << " expect: " << out_size << " actual: " << decompressed_size << std::endl;
		} else {
			out.clear();
		}

		return !out.empty();
	};

	static bool decompress_zstd(const compressd_data_type& in, std::vector<uint8_t>& out) {
		size_t out_size, decompressed_size;

		out.clear();
		if (in.data.empty() || in.raw_size < in.data.size()) return false;

		//out_size = ZSTD_getFrameContentSize(in.data.data(), in.data.size());
		//if (out_size <= 0) out_size = in.raw_size;
		//if (out_size == ZSTD_CONTENTSIZE_ERROR) return false;
		//else if (out_size == ZSTD_CONTENTSIZE_UNKNOWN) out_size = in.size() * 5;

		out.resize(out_size = in.raw_size);

		decompressed_size = ZSTD_decompress(out.data(), out.size(), in.data.data(), in.data.size());
		if (!ZSTD_isError(decompressed_size)) {
			if (decompressed_size != in.raw_size) {
				out.clear();
			}

			std::cout << "in: " << in.data.size() << " expect: " << out_size << " actual: " << decompressed_size << std::endl;
		} else {
			out.clear();
		}

		return !out.empty();
	};
};
