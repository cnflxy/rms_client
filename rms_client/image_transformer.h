#pragma once

#include <string>
#include <vector>
#include <Windows.h>
#include <gdiplus.h>

#pragma comment(lib, "Gdiplus")

class image_transformer {

public:
	image_transformer() = default;

	~image_transformer()
	{
		if (m_inputImage) {
			delete m_inputImage;
		}
	}

	virtual bool set_input(IStream* stream)
	{
		return set_input(Gdiplus::Bitmap::FromStream(stream));
	}

	virtual bool set_input(const std::wstring& url)
	{
		return set_input(Gdiplus::Bitmap::FromFile(url.c_str()));
	}

	virtual bool set_input(HBITMAP hbm, HPALETTE hpal)
	{
		return set_input(Gdiplus::Bitmap::FromHBITMAP(hbm, hpal));
	}

	virtual bool set_input(void* xRGB, int width, int height, int stride, Gdiplus::PixelFormat pixelFormat)
	{
		//auto hbm = CreateBitmap(width, height, 1, bitCount, xRGB);
		//if (!hbm) {
		//	return false;
		//}

		//int bitCount = (pixelFormat >> 8) & 0x7F;

		//if (bitCount > 64 || bitCount & 0x7) return false;

		return set_input(new Gdiplus::Bitmap(width, height, stride, pixelFormat, (BYTE*)xRGB));
	}

	virtual bool output(const std::wstring& url)
	{
		if (m_encoderClsid == CLSID_NULL) {
			set_encoder(L"image/bmp");
			set_quality(100);
		}

		Gdiplus::EncoderParameters encoderParameters;
		encoderParameters.Count = 1;
		encoderParameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
		encoderParameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
		encoderParameters.Parameter[0].NumberOfValues = 1;
		encoderParameters.Parameter[0].Value = &m_quality;
		auto status = m_inputImage->Save(url.c_str(), &m_encoderClsid, &encoderParameters);

		return status == Gdiplus::Ok;
	}

	virtual bool output(IStream** stream)
	{
		if (m_encoderClsid == CLSID_NULL) {
			set_encoder(L"image/bmp");
			set_quality(100);
		}

		auto hr = CreateStreamOnHGlobal(nullptr, TRUE, stream);
		if (FAILED(hr)) return false;

		Gdiplus::EncoderParameters encoderParameters;
		encoderParameters.Count = 1;
		encoderParameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
		encoderParameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
		encoderParameters.Parameter[0].NumberOfValues = 1;
		encoderParameters.Parameter[0].Value = &m_quality;
		auto status = m_inputImage->Save(*stream, &m_encoderClsid, &encoderParameters);
		if (status != Gdiplus::Ok) {
			(*stream)->Release();
			*stream = nullptr;
			return false;
		}

		return true;
	}

	virtual bool output(std::vector<uint8_t>& buffer)
	{
		IStream* stream;

		buffer.clear();

		if (!output(&stream)) return false;

		STATSTG stat;
		auto hr = stream->Stat(&stat, STATFLAG_NONAME);

		if (SUCCEEDED(hr)) {
			hr = stream->Seek({ 0 }, STREAM_SEEK_SET, nullptr);
		}

		if (SUCCEEDED(hr)) {
			buffer.resize(stat.cbSize.QuadPart);
			hr = stream->Read(buffer.data(), stat.cbSize.LowPart, nullptr);
		}

		stream->Release();

		if (FAILED(hr)) {
			buffer.clear();
		}

		return !buffer.empty();
	}

	bool set_encoder(const std::wstring& mime_type) {
		UINT numEncoders = 0, size = 0;

		auto status = Gdiplus::GetImageEncodersSize(&numEncoders, &size);
		if (status != Gdiplus::Ok) return false;
		if (!size || !numEncoders) return false;

		auto pImageEncoderInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(new (std::nothrow) char[size]);
		if (!pImageEncoderInfo) return false;

		bool found = false;
		status = Gdiplus::GetImageEncoders(numEncoders, size, pImageEncoderInfo);
		if (status == Gdiplus::Ok) {
			for (UINT i = 0; i < numEncoders; ++i) {
				if (!mime_type.compare(pImageEncoderInfo[i].MimeType)) {
					m_encoderClsid = pImageEncoderInfo[i].Clsid;
					found = true;
					break;
				}
			}
		}
		delete[] pImageEncoderInfo;

		return found;
	}

	void set_quality(const uint32_t& quality) {
		if (quality > 100) m_quality = 100;
		else m_quality = quality;
	}

private:
	bool set_input(Gdiplus::Bitmap* image)
	{
		if (!image) return false;

		if (m_inputImage) {
			delete m_inputImage;
			m_inputImage = nullptr;
		}

		if (image->GetLastStatus() != Gdiplus::Ok) {
			delete image;
		} else {
			m_inputImage = image;
		}

		return m_inputImage;
	}

private:
	Gdiplus::Bitmap* m_inputImage = nullptr;
	CLSID m_encoderClsid = CLSID_NULL;
	uint32_t m_quality = 100;
};
