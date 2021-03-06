/*
* Copyright 2016 Nu-book Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "BarcodeReader.h"
#include "TextUtfEncoding.h"
#include "GenericLuminanceSource.h"
#include "HybridBinarizer.h"
#include "BinaryBitmap.h"
#include "MultiFormatReader.h"
#include "Result.h"
#include "DecodeHints.h"

#include <windows.h>
#include <gdiplus.h>
#include <type_traits>

namespace ZXing {

BarcodeReader::BarcodeReader(bool tryHarder, bool tryRotate, const std::string& format)
{
	DecodeHints hints;
	hints.setShouldTryHarder(tryHarder);
	hints.setShouldTryRotate(tryRotate);
	if (!format.empty()) {
		BarcodeFormat f = BarcodeFormatFromString(format.c_str());
		if (f != BarcodeFormat::FORMAT_COUNT) {
			hints.setPossibleFormats({ f });
		}
	}
	_reader = std::make_shared<MultiFormatReader>(hints);
}

static std::shared_ptr<LuminanceSource>
CreateLuminanceSource(Gdiplus::Bitmap& bitmap, const Gdiplus::BitmapData& data)
{
	switch (bitmap.GetPixelFormat())
	{
	case PixelFormat24bppRGB:
		return std::make_shared<GenericLuminanceSource>(data.Width, data.Height, data.Scan0, data.Stride, 3, 2, 1, 0);
	case PixelFormat32bppARGB:
	case PixelFormat32bppRGB:
		return std::make_shared<GenericLuminanceSource>(data.Width, data.Height, data.Scan0, data.Stride, 4, 2, 1, 0);
	}
	throw std::invalid_argument("Unsupported format");
}

static std::shared_ptr<BinaryBitmap>
CreateBinaryBitmap(Gdiplus::Bitmap& bitmap)
{
	Gdiplus::BitmapData data;
	bitmap.LockBits(nullptr, Gdiplus::ImageLockModeRead, bitmap.GetPixelFormat(), &data);
	try
	{
		auto result = std::make_shared<HybridBinarizer>(CreateLuminanceSource(bitmap, data));
		bitmap.UnlockBits(&data);
		return result;
	}
	catch (...)
	{
		bitmap.UnlockBits(&data);
		throw;
	}
}

BarcodeReader::ScanResult
BarcodeReader::scan(Gdiplus::Bitmap& bitmap, int rotations)
{
	Result result(DecodeStatus::NotFound);
	auto binImg = CreateBinaryBitmap(bitmap);
	
	if ((rotations & Rotation0) != 0) {
		result = _reader->read(*binImg);
	}
	if (!result.isValid() && (rotations & Rotation180) != 0) {
		result = _reader->read(*binImg->rotated(180));
	}
	if (!result.isValid() && (rotations & RotationCW90) != 0) {
		result = _reader->read(*binImg->rotated(90));
	}
	if (!result.isValid() && (rotations & RotationCCW90) != 0) {
		result = _reader->read(*binImg->rotated(270));
	}
	if (result.isValid()) {
		std::string text;
		TextUtfEncoding::ToUtf8(result.text(), text);
		return{ ToString(result.format()), text };
	}
	return ScanResult();
}

} // ZXing
