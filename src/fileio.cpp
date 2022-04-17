#include "fileio.hpp"

avb::WavReader::WavReader()
{
    status.Clear();
}
avb::WavReader::~WavReader()
{
    status.Clear();
    inputFile.close();
}

void avb::WavReader::Status::Clear()
{
    memset(&hdr, 0, sizeof(hdr));
    valid = false;
    endOfStream = false;
    samplePos = 0;
    totalSamples = 0;
    errorMessage = std::string();
}

bool avb::WavReader::Open(const char* filename)
{
    status.Clear();
    inputFile = std::ifstream(filename, std::ios::binary);
    if(!inputFile.good())
    {
        status.errorMessage = "File not found / corrupted";
        return false;
    }
    inputFile.seekg(0, inputFile.end);
    uint32_t filesize = inputFile.tellg();
    inputFile.seekg(0, inputFile.beg);
    if(filesize < 45)
    {
        status.errorMessage = "File too small to be a valid wav file";
        return false;
    }
    inputFile.read((char*)&status.hdr.riff, sizeof(wav::HdrRiff));
    inputFile.read((char*)&status.hdr.sub1, sizeof(wav::HdrSub1));
    if(status.hdr.sub1.Subchunk1Size > 16)
        inputFile.seekg(status.hdr.sub1.Subchunk1Size-16, inputFile.cur);
    inputFile.read((char*)&status.hdr.sub2, sizeof(wav::HdrSub2));
    if(filesize < (uint32_t)inputFile.tellg()+status.hdr.sub2.Subchunk2Size)
    {
        status.errorMessage = "File smaller than header suggests";
        return false;
    }

    if(!(status.hdr.sub1.BitsPerSample == 8
    || status.hdr.sub1.BitsPerSample == 16
    || status.hdr.sub1.BitsPerSample == 24
    || status.hdr.sub1.BitsPerSample == 32))
    {
        status.errorMessage = "Unsupported bit depth";
        return false;
    }

    if(!(status.hdr.sub1.NumChannels == 1
    || status.hdr.sub1.NumChannels == 2))
    {
        status.errorMessage = "Only mono and stereo files supported";
    }

    if(status.hdr.riff.ChunkID     != 0x46464952 //RIFF
    || status.hdr.riff.Format      != 0x45564157 //WAVE
    || status.hdr.sub1.Subchunk1ID != 0x20746D66 //fmt
    || status.hdr.sub2.Subchunk2ID != 0x61746164) //data
    {
        status.errorMessage = "Magic number invalid";
        return false;
    }

    status.valid = true;
    status.totalSamples = status.hdr.sub2.Subchunk2Size / status.hdr.sub1.BlockAlign;
    return true;
}

std::vector<std::valarray<uint8_t>> avb::WavReader::ReadBlock8(uint32_t len)
{
    uint32_t numCh = status.hdr.sub1.NumChannels;
    std::vector<uint8_t> readBuf(len*numCh, 0);
    std::vector<std::valarray<uint8_t>> r(numCh);

    uint32_t lenRead = std::min(len, status.totalSamples-status.samplePos);
    inputFile.read((char*)&readBuf[0], lenRead*numCh*sizeof(uint8_t));
    for(uint32_t i=0; i<numCh; i++)
    {
        r[i] = std::valarray<uint8_t>(len);
        for(uint32_t j=0; j<len; j++)
        {
            r[i][j] = readBuf[j*numCh+i];
        }
    }
    status.samplePos += lenRead;
    return r;
}
std::vector<std::valarray<int16_t>> avb::WavReader::ReadBlock16(uint32_t len)
{
    uint32_t numCh = status.hdr.sub1.NumChannels;
    std::vector<int16_t> readBuf(len*numCh, 0);
    std::vector<std::valarray<int16_t>> r(numCh);

    uint32_t lenRead = std::min(len, status.totalSamples-status.samplePos);
    inputFile.read((char*)&readBuf[0], lenRead*numCh*sizeof(int16_t));
    for(uint32_t i=0; i<numCh; i++)
    {
        r[i] = std::valarray<int16_t>(len);
        for(uint32_t j=0; j<len; j++)
        {
            r[i][j] = readBuf[j*numCh+i];
        }
    }
    status.samplePos += lenRead;
    return r;
}
std::vector<std::valarray<int32_t>> avb::WavReader::ReadBlock24(uint32_t len)
{
    uint32_t numCh = status.hdr.sub1.NumChannels;
    std::vector<uint8_t> readBuf(len*numCh*3 + 4, 0);
    std::vector<std::valarray<int32_t>> r(numCh);

    uint32_t lenRead = std::min(len, status.totalSamples-status.samplePos);
    inputFile.read((char*)&readBuf[0], lenRead*numCh*3);
    for(uint32_t i=0; i<numCh; i++)
    {
        r[i] = std::valarray<int32_t>(len);
        for(uint32_t j=0; j<len; j++)
        {
            int32_t smpReal;
            uint32_t smp = *reinterpret_cast<uint32_t*>(&readBuf[3*(j*numCh+i)]);
            smp &= 0x00FFFFFF;
            smpReal = smp;
            if(smp & 0x00800000)
            {
                smp = ~smp;
                smp &= 0x00FFFFFF;
                smp += 1;
                smpReal = -smp;
            }
            r[i][j] = smpReal;
        }
    }
    status.samplePos += lenRead;
    return r;
}
std::vector<std::valarray<float>> avb::WavReader::ReadBlock32f(uint32_t len)
{
    uint32_t numCh = status.hdr.sub1.NumChannels;
    std::vector<float> readBuf(len*numCh, 0);
    std::vector<std::valarray<float>> r(numCh);

    uint32_t lenRead = std::min(len, status.totalSamples-status.samplePos);
    inputFile.read((char*)&readBuf[0], lenRead*numCh*sizeof(float));
    for(uint32_t i=0; i<numCh; i++)
    {
        r[i] = std::valarray<float>(len);
        for(uint32_t j=0; j<len; j++)
        {
            r[i][j] = readBuf[j*numCh+i];
        }
    }
    status.samplePos += lenRead;
    return r;
}

std::vector<std::valarray<float>> avb::WavReader::ReadBlock(uint32_t len)
{
    uint32_t numCh = status.hdr.sub1.NumChannels;
    std::vector<std::valarray<float>> r(numCh);
    for(uint32_t i=0; i<numCh; i++)
        r[i] = std::valarray<float>(len);
    if(status.hdr.sub1.BitsPerSample == 8)
    {
        std::vector<std::valarray<uint8_t>> b = ReadBlock8(len);
        for(uint32_t i=0; i<numCh; i++)
            for(uint32_t j=0; j<len; j++)
                r[i][j] = ((float)b[i][j] - 128.0f) / 128.0f;
        return r;
    }
    else if(status.hdr.sub1.BitsPerSample == 16)
    {
        std::vector<std::valarray<int16_t>> b = ReadBlock16(len);
        for(uint32_t i=0; i<numCh; i++)
            for(uint32_t j=0; j<len; j++)
                r[i][j] = (float)b[i][j] / 32768.0f;
        return r;
    }
    else if(status.hdr.sub1.BitsPerSample == 24)
    {
        std::vector<std::valarray<int32_t>> b = ReadBlock24(len);
        for(uint32_t i=0; i<numCh; i++)
            for(uint32_t j=0; j<len; j++)
                r[i][j] = (float)b[i][j] / 8388608.0f;
        return r;
    }
    else if(status.hdr.sub1.BitsPerSample == 32)
    {
        return ReadBlock32f(len);
    }
    return r;
}

uint32_t avb::WavReader::Buffer(uint32_t blockSize, uint32_t blockCount)
{
    if(!status.valid)
        return 0;
    uint32_t numCh = status.hdr.sub1.NumChannels;
    buffer.resize(numCh);
    for(uint32_t i=0; i<numCh; i++)
    {
        buffer[i] = std::vector<std::valarray<float>>();
        buffer[i].reserve(blockCount);
    }
    for(uint32_t i=0; i<blockCount && status.samplePos < status.totalSamples; i++)
    {
        std::vector<std::valarray<float>> block = ReadBlock(blockSize);
        for(uint32_t j=0; j<numCh; j++)
        {
            buffer[j].push_back(block[j]);
        }
    }
    if(status.samplePos >= status.totalSamples)
    {
        status.endOfStream = true;
    }
    return buffer[0].size();
}

std::vector<std::valarray<float>> avb::WavReader::GetBuffer(uint16_t channel)
{
    return buffer.at(channel);
}

std::valarray<float> avb::WavReader::GetBufferedBlock(uint16_t channel, uint32_t block)
{
    return buffer[channel][block];
}

void avb::WavReader::Close()
{
    inputFile.close();
    status.Clear();
}

avb::RawImgReader16::RawImgReader16()
{
}
avb::RawImgReader16::~RawImgReader16()
{
}

uint32_t avb::RawImgReader16::GetImageHeight()
{
    return imageHeight;
}
void avb::RawImgReader16::CopyBufferLine(uint32_t dst, uint32_t src)
{
    if(dst==src)
        return;
    memcpy(&buffer[scanlineSize*dst], &buffer[scanlineSize*src], scanlineSize*sizeof(Pixel16));
}
void avb::RawImgReader16::ShiftBuffer(int32_t numLines)
{
    if(numLines > 0)
    {
        for(int32_t i=0; i<(int32_t)bufferLines-numLines; i++)
            CopyBufferLine(i, i+numLines);
        for(int32_t i=(int32_t)bufferLines-numLines; i<(int32_t)bufferLines; i++)
            memset(&buffer[scanlineSize*i], 0, scanlineSize*sizeof(Pixel16));
    }
    if(numLines < 0)
    {
        for(int32_t i=bufferLines-1; i>=-numLines; i--)
            CopyBufferLine(i, i+numLines);
        for(int32_t i=0; i<-numLines; i++)
            memset(&buffer[scanlineSize*i], 0, scanlineSize*sizeof(Pixel16));
    }
}

void avb::RawImgReader16::SetBufferPos(uint32_t startLine)
{
    std::pair<int32_t,int32_t> oldPos(bufferPos, bufferPos+bufferLines);
    std::pair<int32_t,int32_t> newPos(startLine, startLine+bufferLines);
    int32_t maxReadLines = imageHeight-newPos.first;
    newPos.second = std::min(newPos.second, (int32_t)imageHeight);
    bufferPos = startLine;

    if(newPos.first < oldPos.first && newPos.second > oldPos.first)
    {
        uint32_t linesToRead = std::min(maxReadLines, oldPos.first-newPos.first);
        ShiftBuffer(newPos.first-oldPos.first);
        inputFile.seekg(headerSize+scanlineSize*newPos.first*sizeof(Pixel16), inputFile.beg);
        inputFile.read((char*)&buffer[0], linesToRead*sizeof(Pixel16)*scanlineSize);
        return;
    }
    else if(oldPos.first < newPos.first && oldPos.second > newPos.first)
    {
        uint32_t linesToRead = std::min(maxReadLines, newPos.first-oldPos.first);
        ShiftBuffer(newPos.first-oldPos.first);
        inputFile.seekg(headerSize+scanlineSize*oldPos.second*sizeof(Pixel16), inputFile.beg);
        inputFile.read((char*)&buffer[scanlineSize*(oldPos.second-newPos.first)], linesToRead*sizeof(Pixel16)*scanlineSize);
        return;
    }
    else
    {
        uint32_t linesToRead = std::min(maxReadLines, (int32_t)bufferLines);
        memset(&buffer[0], 0, buffer.size()*sizeof(Pixel16));
        inputFile.seekg(headerSize+scanlineSize*newPos.first*sizeof(Pixel16), inputFile.beg);
        inputFile.read((char*)&buffer[0], sizeof(Pixel16)*scanlineSize*linesToRead);
    }
}

void avb::RawImgReader16::UpdateBufferPos(uint32_t line)
{
    uint32_t newPos = line-(line%(bufferLines/2));
    if(newPos >= bufferLines/4)
        newPos -= bufferLines/4;
    if(bufferPos != newPos)
        SetBufferPos(newPos);
    return;
}

bool avb::RawImgReader16::Open(const char* filename, uint32_t scanlineSize, uint32_t headerSize, void* headerPtr)
{
    Close();
    uint32_t sz = FileSize(filename);
    inputFile.open(filename, std::ios::binary);
    if(!inputFile.good())
        return false;
    if(!headerPtr)
        inputFile.seekg(headerSize, inputFile.beg);
    else
        inputFile.read((char*)headerPtr, headerSize);
    if(inputFile.eof())
        return false;
    bufferLines = 256;
    this->scanlineSize = scanlineSize;
    this->headerSize = headerSize;
    imageHeight = ((sz-headerSize)/sizeof(Pixel16))/scanlineSize;
    buffer = std::vector<Pixel16>(scanlineSize*bufferLines);
    memset(&buffer[0], 0, sizeof(Pixel16)*buffer.size());
    bufferPos = 0;
    SetBufferPos(0);
    return true;
}

void avb::RawImgReader16::Close()
{
    inputFile.close();
    buffer = std::vector<Pixel16>();
    bufferLines = 0;
    bufferPos = 0;
    scanlineSize = 0;
    headerSize = 0;
}

void avb::RawImgReader16::GetScanline(Pixel16* out, int32_t y)
{
    memset(out, 0, sizeof(Pixel16)*scanlineSize);
    if(y < 0 || y > (int32_t)imageHeight)
        return;
    UpdateBufferPos(y);
    memcpy(out, &buffer[scanlineSize*(y-bufferPos)], sizeof(Pixel16)*scanlineSize);
}

uint32_t avb::FileSize(const char* filename)
{
    std::ifstream f(filename, std::ios::binary);
    if(!f.good())
        return 0;
    f.seekg(0, f.end);
    return f.tellg();
}
bool avb::FileExists(const char* filename)
{
    std::ifstream f(filename);
    return f.good();
}

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#endif // __linux__

bool avb::CreateCustomSizedFile(const char* filename, uint32_t sz)
{
#ifdef _WIN32
    return CreateCustomSizedFileWindows(filename, sz);
#elif defined(__linux__)
    return CreateCustomSizedFileLinux(filename, sz);
#else
    uint32_t bufsize=1024576, saved=0, left=sz;
    std::vector<char> buf(bufsize,0);
    std::ofstream file(filename, std::ios::binary);
    while(left)
    {
        uint32_t n = std::min(bufsize, left);
        if(!file.write(&buf[0], n))
            return false;
        saved += n;
        left -= n;
    }
    return true;
#endif
}

bool avb::CreateCustomSizedFileWindows(const char* filename, uint32_t sz)
{
#ifdef _WIN32
    if(FileExists(filename))
        DeleteFile(filename);
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER dist;
    int64_t distc = sz;
    memcpy(&dist,&distc,sizeof(int64_t));
    DWORD err = SetFilePointerEx(hFile, dist, NULL, FILE_BEGIN);
    if (err == INVALID_SET_FILE_POINTER || !SetEndOfFile(hFile))
    {
        DeleteFile(filename);
        return false;
    }
    SetFilePointer(hFile, 0, 0, FILE_BEGIN);
    CloseHandle(hFile);
    return true;
#endif // _WIN32
    return false;
}

bool avb::CreateCustomSizedFileLinux(const char* filename, uint32_t sz) //NOT TESTED
{
#ifdef __linux__
    FILE* f = fopen(filename, "wb");
    int r = fallocate(fileno(f), 0, 0, sz);
    fclose(f);
    return r==0;
#endif // __linux__
    return false;
}
