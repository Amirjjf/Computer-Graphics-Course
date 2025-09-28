#pragma once

#include <cassert>
#include <fstream>
#include <fmt/core.h>

#include "lodepng.h"

#include "Utils.h"
#include "vec_utils.h"

// --------------------------------------------------------------------------

typedef Vector<uint8_t, 4> Vector4u8;

// --------------------------------------------------------------------------
// 
// Actual image class
template<class PixelType>
class ImageBase
{
public:
    ImageBase(const Vector2i& size, const PixelType& initializer)
    {
        size_ = size;
        data_.resize(size(0) * size(1), initializer);
    }

    shared_ptr<ImageBase<Vector<uint8_t, 4>>> to_uint8() const;

    void exportPNG(const filesystem::path& filename) const
    {
        shared_ptr<ImageBase<Vector<uint8_t, 4>>> u8img = to_uint8();
        vector<uint8_t> eightbit((uint8_t*)u8img->data(), (uint8_t*)(u8img->data() + size_(0) * size_(1)));
        vector<uint8_t> out;
        lodepng::State S;
        unsigned error = lodepng::encode(out, eightbit, size_(0), size_(1), S);
        if(error)
            fail(fmt::format("PNG encoder error: {}", lodepng_error_text(error)));
        ofstream file(filename.string().c_str(), std::ios::binary);
        file.write(reinterpret_cast<const char*>(out.data()), out.size());
    }

    static ImageBase<Vector4u8> loadPNG(const string& filename)
    {
        std::vector<unsigned char> image; //the raw pixels
        unsigned width, height;

        //decode
        unsigned error = lodepng::decode(image, width, height, filename.c_str());

        //if there's an error, display it
        if (error)
            fail(fmt::format("loadPNG({}):\nlonepng decoder error {}: {}", filename, error, lodepng_error_text(error)));

        ImageBase<Vector4u8> img(Vector2i(width, height), Vector4u8::Zero());
        memcpy(img.data_.data(), image.data(), width * height * 4);
        return img;
    }

    inline Vector2i             getSize() const                 { return size_; }
    inline PixelType&           pixel(int x, int y)             { return data_[y * size_(0) + x]; }
    inline const PixelType&     pixel(int x, int y) const       { return data_[y * size_(0) + x]; }
    inline PixelType*           data()                          { return data_.data(); }
    inline const PixelType*     data() const                    { return data_.data(); }

protected:
    vector<PixelType>     data_;
    Vector2i              size_;
};

// --------------------------------------------------------------------------

template<class PixelType>
shared_ptr<ImageBase<Vector<uint8_t, 4>>> ImageBase<PixelType>::to_uint8() const
{
    Vector<uint8_t, 4> initializer = Vector<uint8_t, 4>{ 0, 0, 0, 255 };
    auto u8img = make_shared<ImageBase<Vector<uint8_t, 4>>>(size_, initializer);
    for (int j = 0; j < size_(1); ++j)
        for (int i = 0; i < size_(0); ++i)
            for (int d = 0; d < min(4, (int)PixelType::RowsAtCompileTime); ++d)
                if ( is_floating_point_v<decltype(pixel(i, j)[d])>)
                    u8img->pixel(i, j)[d] = uint8_t(max(0.0f, min(255.0f, pixel(i, j)[d]*255.0f)));
                else
                    u8img->pixel(i, j)[d] = pixel(i, j)[d];
                //u8img->pixel(i, j)[d] = pixel_traits_t<PixelType::Scalar>::to_uint8(pixel(i, j)[d]);
    return u8img;
}

// --------------------------------------------------------------------------

typedef ImageBase<float> Image1f;
typedef ImageBase<Vector2f> Image2f;
typedef ImageBase<Vector3f> Image3f;
typedef ImageBase<Vector4f> Image4f;
typedef ImageBase<Vector4u8> Image4u8;
