#ifndef AVB_FILEIO_H
#define AVB_FILEIO_H

#include "incl/c_cpp.hpp"

namespace avb
{
    struct Pixel16
    {
        uint16_t r,g,b;
    };
    namespace wav
    {
        struct HdrRiff
        {
            uint32_t ChunkID;
            uint32_t ChunkSize;
            uint32_t Format;
        };
        struct HdrSub1
        {
            uint32_t Subchunk1ID;
            uint32_t Subchunk1Size;
            uint16_t AudioFormat;
            uint16_t NumChannels;
            uint32_t SampleRate;
            uint32_t ByteRate;
            uint16_t BlockAlign;
            uint16_t BitsPerSample;
        };
        struct HdrSub2
        {
            uint32_t Subchunk2ID;
            uint32_t Subchunk2Size;
        };
        struct Header
        {
            HdrRiff riff;
            HdrSub1 sub1;
            HdrSub2 sub2;
        };
    }
    class WavReader
    {
        std::ifstream inputFile;
        std::vector<std::vector<std::valarray<float>>> buffer;

        std::vector<std::valarray<uint8_t>> ReadBlock8(uint32_t len);
        std::vector<std::valarray<int16_t>> ReadBlock16(uint32_t len);
        std::vector<std::valarray<int32_t>> ReadBlock24(uint32_t len);
        std::vector<std::valarray<float>> ReadBlock32f(uint32_t len);
        
        struct Status
        {
            void Clear();
            wav::Header hdr;
            bool valid;
            bool endOfStream;
            uint32_t samplePos;
            uint32_t totalSamples;
            std::string errorMessage;
        };
    public:
        Status status;
        WavReader();
        ~WavReader();
        std::vector<std::valarray<float>> ReadBlock(uint32_t len);

        bool Open(const char* filename);
        uint32_t Buffer(uint32_t blockSize, uint32_t blockCount);
        std::vector<std::valarray<float>> GetBuffer(uint16_t channel);
        std::valarray<float> GetBufferedBlock(uint16_t channel, uint32_t block);
        void Close();
    };
    class RawImgReader16
    {
        std::ifstream inputFile;
        std::vector<Pixel16> buffer;
        uint32_t bufferLines;
        uint32_t bufferPos;
        uint32_t scanlineSize;
        uint32_t headerSize;
        uint32_t imageHeight;
        void CopyBufferLine(uint32_t dst, uint32_t src);
        void ShiftBuffer(int32_t numLines);
        void SetBufferPos(uint32_t startLine);
        void UpdateBufferPos(uint32_t line);
    public:
        RawImgReader16();
        ~RawImgReader16();
        uint32_t GetImageHeight();
        bool Open(const char* filename, uint32_t scanlineSize, uint32_t headerSize, void* headerPtr);
        void Close();
        void GetScanline(Pixel16* out, int32_t y);
    };
    
    uint32_t FileSize(const char* filename);
    bool FileExists(const char* filename);
    
    bool CreateCustomSizedFile(const char* filename, uint32_t sz);
    bool CreateCustomSizedFileWindows(const char* filename, uint32_t sz);
    bool CreateCustomSizedFileLinux(const char* filename, uint32_t sz);
}

#endif // AVB_FILEIO_H
