#include "converter.hpp"

avb::ImageFileHeader avb::MakeBlankImageFileHeader()
{
    ImageFileHeader r;
    memset(&r, 0, sizeof(r));
    r.magicNumber = 0x42069AB6;
    r.headerVersion = 1;
    r.headerSize = sizeof(r);
    return r;
}
avb::ConverterSettings avb::MakeDefaultConverterSettings()
{
    ConverterSettings r;
    //r.magicNumber = 0x42069AB6;
    //r.headerVersion = 1;
    r.fftSize = 2048;
    r.compandingMethod = AVB_COMPANDING_UV_LAW;
    r.windowFunction = AVB_WINDOW_BLACKMAN_HARRIS;
    r.companderParam[0] = 768.0f;
    r.companderParam[1] = 0.125f;
    r.horizontalTime = false;
    r.forceOverwrite = false;
    r.outputFloat32Audio = false;
    return r;
}

avb::ForwardConverterThread::ForwardConverterThread()
{
    fftwPlanExists = false;
    fftwAudioBuffer = nullptr;
    fftwDFTBuffer = nullptr;
    //Init(MakeDefaultConverterSettings());
}
avb::ForwardConverterThread::ForwardConverterThread(avb::ConverterSettings t_settings)
{
    fftwPlanExists = false;
    fftwAudioBuffer = nullptr;
    fftwDFTBuffer = nullptr;
    Init(t_settings);
}
avb::ForwardConverterThread::~ForwardConverterThread()
{
    Deinit();
}

bool avb::ForwardConverterThread::Init(avb::ConverterSettings t_settings)
{
    settings = t_settings;
    uint32_t bins = settings.fftSize/2+1;

    outTmpReal = outTmpImag = outTmpMagn = std::valarray<float>(bins);
    outTmpReal16 = outTmpImag16 = std::valarray<uint16_t>(bins);
    outTmpMagn16 = std::valarray<uint16_t>(bins);
    fftwAudioBuffer = (float*)fftwf_malloc(2*bins*sizeof(float));
    fftwDFTBuffer = (fftwf_complex*)fftwf_malloc(bins*sizeof(fftwf_complex));
    fftwPlan = fftwf_plan_dft_r2c_1d(settings.fftSize, fftwAudioBuffer, fftwDFTBuffer, FFTW_MEASURE);
    fftwPlanExists = true;
    window = MakeWindow(settings.fftSize, settings.windowFunction);

    compressor.Init(t_settings.compandingMethod, t_settings.companderParam);
    if(!fftwAudioBuffer || !fftwDFTBuffer)
        return false;
    return true;
}
void avb::ForwardConverterThread::Deinit()
{
    if(fftwPlanExists)
    {
        fftwf_destroy_plan(fftwPlan);
        fftwPlanExists = false;
    }
    fftwf_free(fftwAudioBuffer);
    fftwf_free(fftwDFTBuffer);
}

void avb::ForwardConverterThread::ProcessBlock(Pixel16* output, std::valarray<float>& input)
{
    uint32_t bins = (settings.fftSize/2+1);
    input *= window;
    memcpy(fftwAudioBuffer, &input[0], settings.fftSize*sizeof(float));
    fftwf_execute(fftwPlan);
    for(uint32_t i=0; i<bins; i++)
        outTmpReal[i] = fftwDFTBuffer[i][0];
    for(uint32_t i=0; i<bins; i++)
        outTmpImag[i] = fftwDFTBuffer[i][1];
    outTmpReal /= float(settings.fftSize/2);
    outTmpImag /= float(settings.fftSize/2);
    outTmpMagn = outTmpReal*outTmpReal + outTmpImag*outTmpImag;
    outTmpMagn = std::sqrt(outTmpMagn);
    outTmpReal /= outTmpMagn;
    outTmpImag /= outTmpMagn;
    outTmpReal16 = compfn::F32toUI16(outTmpReal*0.5f + 0.5f, true);
    outTmpImag16 = compfn::F32toUI16(outTmpImag*0.5f + 0.5f, true);
    outTmpMagn16 = compressor.Compress(outTmpMagn);
    for(uint32_t i=0; i<bins; i++)
    {
        output[i].r = outTmpReal16[i];
        output[i].b = outTmpImag16[i];
        output[i].g = outTmpMagn16[i];
    }
    //puts("ProcessBlock done");
}

void avb::ForwardConverterThread::Process()
{
    uint32_t bins = (settings.fftSize/2+1);
    outputs.resize(inputs.size()*bins);
    for(uint32_t i=0; i<inputs.size(); i++)
        ProcessBlock(&outputs[i*bins], inputs[i]);
    inputs.resize(0);
}

void avb::ForwardConverterThread::ProcessAsync()
{
    stlThread = std::thread(Process, this);
}
void avb::ForwardConverterThread::WaitForCompletion()
{
    stlThread.join();
}

avb::ForwardConverter::ForwardConverter()
{
    numThreads = 0;
}
avb::ForwardConverter::~ForwardConverter()
{
    Destroy();
}

bool avb::ForwardConverter::isNumber7smooth(uint32_t n)
{
    int primes[4] = {2, 3, 5, 7};
    for(int i=0; i<4; i++)
        while(n%primes[i] == 0)
            n /= primes[i];
    if(n > 1)
        return false;
    return true;
}
std::string avb::ForwardConverter::RemoveFilenameExtension(std::string s)
{
    unsigned lastDotPos = s.size()-1;
    for(unsigned i=s.size()-1; i>0; i--)
    {
        if(s[i] == '.')
        {
            lastDotPos = i;
            break;
        }
    }
    return s.substr(0, lastDotPos);

}

bool avb::ForwardConverter::Init(avb::ConverterSettings t_settings)
{
    printf("Initializing converter...\n");

    numThreads = std::thread::hardware_concurrency();
    printf("Logical processors available: %d ", numThreads);
    if(numThreads%2 == 1)
    {
        numThreads *= 2;
        printf("(will create %d threads)", numThreads);
    }
    printf("\n");

    settings = t_settings;

    thr = std::vector<ForwardConverterThread>(numThreads);
    printf("Initializing thread ");

    for(uint32_t i=0; i<thr.size(); i++)
    {
        printf("%d.. ", i+1);
        if(!thr[i].Init(t_settings))
        {
            printf("Failed to init thread\n");
            return false;
        }
    }
    printf("\n");
    return true;

}

void WriteInBlocks(FILE* f, void* dat, uint32_t dataSize, uint32_t blockSize)
{
    uint32_t bytesLeft = dataSize;
    uint32_t bytesSaved = 0;
    while(bytesLeft)
    {
        uint32_t bytesToSave = std::min(blockSize, bytesLeft);
        fwrite((char*)dat+bytesSaved, 1, bytesToSave, f);
        bytesLeft -= bytesToSave;
        bytesSaved += bytesToSave;
    }
}
void WriteHelper(std::vector<FILE*> f, std::vector<void*> d, std::vector<uint32_t> s)
{
    for(uint32_t i=0; i<f.size(); i++)
        WriteInBlocks(f[i], d[i], s[i], 524288);
}

bool avb::ForwardConverter::Convert(const char* inputFilename)
{
    //TODO: break up this fucking 120-line monster of a function
    printf("\nStarting conversion... (input filename: %s)\n", inputFilename);
    bool wavValid = audioReader.Open(inputFilename);
    if(!wavValid)
    {
        printf("Could not open WAV file: %s\n", audioReader.status.errorMessage.c_str());
        return false;
    }
    puts("WAV file ok");

    uint32_t numCh = audioReader.status.hdr.sub1.NumChannels;
    uint32_t blocksProcessed = 0;
    uint32_t fftSize = settings.fftSize;
    uint32_t totalBlocks = (audioReader.status.totalSamples+(fftSize/2-1))/(fftSize/2);
    uint32_t blockCount = std::min(4096U, std::max(64U, totalBlocks/8));
    uint32_t threads = thr.size();
    uint32_t threadsPerCh = threads/numCh;
    uint32_t prevProgressMsgLength = 0;
    if(numCh > 2)
    {
        printf("sorry, more than 2 channels not supported\n");
        return false;
    }
    std::vector<FILE*> outFile(numCh);
    printf("Allocating hard drive space... ");
    for(uint32_t i=0; i<numCh; i++)
    {
        //i apologize for this in advance
        std::string outFnTmp = RemoveFilenameExtension(std::string(inputFilename));
        std::vector<char> realFNBuf(outFnTmp.size()+69, 0);
        sprintf(&realFNBuf[0], "%s_ch%d.raw", outFnTmp.c_str(), i+1);
        uint32_t fileSizeBytes = totalBlocks*(fftSize/2+1)*sizeof(Pixel16) + sizeof(ImageFileHeader);
        bool fileCreated = CreateCustomSizedFile(&realFNBuf[0], fileSizeBytes);
        if(!fileCreated)
        {
            printf("failure\n");
            return false;
        }
        outFile[i] = fopen(&realFNBuf[0], "r+b");
    }
    puts("");
    std::vector<std::valarray<float>> inputBuf(numCh);
    for(uint32_t i=0; i<numCh; i++)
        inputBuf[i] = std::valarray<float>(0.0f, fftSize);
    bool firstIteration = true;
    printf("Converting... ");

    for(uint32_t i=0; i<numCh; i++)
    {
        ImageFileHeader h = MakeBlankImageFileHeader();
        h.convSettingsUsed = settings;
        h.inputWavHeader = audioReader.status.hdr;
        fwrite(&h, sizeof(h), 1, outFile[i]);
    }

    for(uint32_t it=0; ; it++)
    {
        if(thr[0].outputsLast.size() == 0 && audioReader.status.endOfStream)
            break;
        for(uint32_t i=0; i<threads; i++)
        {
            thr[i].inputs = thr[i].inputsNext;
            thr[i].inputsNext.resize(0);
        }
        for(uint32_t i=0; i<threads && !firstIteration; i++)
            thr[i].ProcessAsync();
        std::vector<FILE*> writeFilePtr(threads);
        std::vector<void*> writePtr(threads);
        std::vector<uint32_t> writeSizes(threads);
        for(uint32_t t=0; t<threads && !firstIteration; t++)
        {
            writeFilePtr[t] = outFile[t/threadsPerCh];
            writePtr[t] = &thr[t].outputsLast[0];
            writeSizes[t] = sizeof(Pixel16)*thr[t].outputsLast.size();
        }
        uint32_t blocksRead = audioReader.Buffer(fftSize/2, blockCount);
        std::thread writeThread(WriteHelper, writeFilePtr, writePtr, writeSizes);
        for(uint32_t i=0; i<numCh; i++)
        {
            for(uint32_t j=0; j<blocksRead; j++)
            {
                //buffer -> thread inputs
                std::valarray<float> block = audioReader.GetBufferedBlock(i, j);
                inputBuf[i] = inputBuf[i].shift(fftSize/2);
                memcpy(&inputBuf[i][fftSize/2], &block[0], sizeof(float)*fftSize/2);
                uint32_t blockThread = (j*threadsPerCh) / blockCount;
                blockThread += i*threadsPerCh;
                thr[blockThread].inputsNext.push_back(inputBuf[i]);
            }
        }
        writeThread.join();
        for(uint32_t i=0; i<thr.size() && !firstIteration; i++)
            thr[i].WaitForCompletion();
        for(uint32_t i=0; i<threads && !firstIteration; i++)
        {
            thr[i].outputsLast = thr[i].outputs;
            thr[i].outputs.resize(0);
        }
        firstIteration = false;
        for(uint32_t i=0; i<prevProgressMsgLength; i++)
            printf("\b");
        char pmBuf[80];
        sprintf(pmBuf, "%d/%d", blocksProcessed, totalBlocks);
        printf("%s", pmBuf);
        prevProgressMsgLength = strlen(pmBuf);
        blocksProcessed += blocksRead;
    }
    for(auto& f : outFile)
        fclose(f);
    puts("");
    printf("Conversion completed.\n\n");

    printf("Open the RAW image(s) in your editor of choice with these settings:\n\n");
    printf("Header size: %d bytes (for PS, remember to check \"retain while saving\")\n", sizeof(ImageFileHeader));
    printf("Byte order: little-endian (IBM PC, Intel)\n");
    printf("Channels: 3 (interleaved)\n");
    printf("Depth: R16G16B16 (48bpp)\n");
    printf("Dimensions: %dx%d\n", fftSize/2+1, totalBlocks);
    return true;
}

void avb::ForwardConverter::Destroy()
{
    numThreads = 0;
    thr = std::vector<ForwardConverterThread>();
    audioReader.Close();
}



std::vector<std::string> avb::FindMatchingFilenamesBC(const char* name)
{
    std::vector<std::string> r;
    for(int i=0;;i++)
    {
        std::vector<char> guess(strlen(name)+69, 0);
        sprintf(&guess[0], "%s_ch%d.raw", name, i+1);
        if(!FileExists(&guess[0]))
            break;
        r.push_back(std::string(&guess[0]));
    }
    if(r.size())
        return r;
    std::string name_s(name);
    auto p = name_s.rfind("_ch");
    if(p == std::string::npos)
        return std::vector<std::string>();
    std::string name_short = name_s.substr(0, p);

    for(int i=0;;i++)
    {
        std::vector<char> guess(name_short.size()+69, 0);
        sprintf(&guess[0], "%s_ch%d.raw", name_short.c_str(), i+1);
        if(!FileExists(&guess[0]))
            break;
        r.push_back(std::string(&guess[0]));
    }
    return r;
}

avb::BackwardConverterThread::BackwardConverterThread()
{
    fftwPlanExists = false;
    fftwAudioBuffer = nullptr;
    fftwDFTBuffer = nullptr;
    //Init(MakeDefaultConverterSettings());
}
avb::BackwardConverterThread::BackwardConverterThread(avb::ConverterSettings t_settings)
{
    fftwPlanExists = false;
    fftwAudioBuffer = nullptr;
    fftwDFTBuffer = nullptr;
    Init(t_settings);
}
avb::BackwardConverterThread::~BackwardConverterThread()
{
    Deinit();
}

bool avb::BackwardConverterThread::Init(ConverterSettings t_settings)
{
    settings = t_settings;
    uint32_t bins = t_settings.fftSize/2+1;
    fftwAudioBuffer = (float*)fftwf_malloc(2*bins*sizeof(float));
    fftwDFTBuffer = (fftwf_complex*)fftwf_malloc(bins*sizeof(fftwf_complex));
    fftwPlan = fftwf_plan_dft_c2r_1d(settings.fftSize, fftwDFTBuffer, fftwAudioBuffer, FFTW_MEASURE);
    fftwPlanExists = true;
    window = MakeWindow(settings.fftSize, settings.windowFunction);
    inverseSquareWindow = window*window;
    inverseSquareWindow += inverseSquareWindow.cshift(settings.fftSize/2);
    inverseSquareWindow = 1.0f / inverseSquareWindow;
    expander.Init(t_settings.compandingMethod, t_settings.companderParam);
    return true;
}
void avb::BackwardConverterThread::Deinit()
{
    if(fftwPlanExists)
    {
        fftwf_destroy_plan(fftwPlan);
        fftwPlanExists = false;
    }
    fftwf_free(fftwAudioBuffer);
    fftwf_free(fftwDFTBuffer);
}
void avb::BackwardConverterThread::ProcessBlock(float* out, RawImgReader16* reader, int32_t centerBlockPos)
{
    uint32_t numBins = settings.fftSize/2+1;
    std::valarray<float> bf[3];
    std::valarray<float> res;
    for(int i=0; i<3; i++)
        bf[i] = std::valarray<float>(settings.fftSize);
    for(int i=0; i<3; i++)
    {
        std::vector<Pixel16> line(numBins);
        std::valarray<uint16_t> magn16(numBins);
        std::valarray<float> real, imag, magn;
        real = imag = std::valarray<float>(numBins);
        reader->GetScanline(&line[0], centerBlockPos+i-1);
        for(uint32_t i=0; i<numBins; i++)
        {
            real[i] = (float)line[i].r;
            imag[i] = (float)line[i].b;
            magn16[i] = line[i].g;
        }
        real = (real-32768.0f)/32768.0f;
        imag = (imag-32768.0f)/32768.0f;
        magn = expander.Expand(magn16);
        real *= magn;
        imag *= magn;
        for(uint32_t i=0; i<numBins; i++)
        {
            fftwDFTBuffer[i][0] = real[i];
            fftwDFTBuffer[i][1] = imag[i];
        }
        fftwf_execute(fftwPlan);
        memcpy(&(bf[i][0]), fftwAudioBuffer, sizeof(float)*settings.fftSize);
        bf[i] *= window;
        bf[i] *= settings.fftSize*8;
    }
    res = bf[1];
    res += bf[0].shift(settings.fftSize/2);
    res += bf[2].shift(-(int)settings.fftSize/2);
    res *= inverseSquareWindow;
    memcpy(out, &res[0], sizeof(float)*settings.fftSize);
}

bool avb::BackwardConverter::Convert(const char* name)
{
    std::vector<std::string> names = FindMatchingFilenamesBC(name);
    if(!names.size())
    {
        printf("File not found\n");
        return false;
    }
    numCh = names.size();
    numThreads = std::thread::hardware_concurrency();
    if(numThreads%2 == 1)
        numThreads *= 2;
    thr = std::vector<BackwardConverterThread>(numThreads);
    imgReader = std::vector<RawImgReader16>(numCh);
    std::vector<ImageFileHeader> chHdr(numCh);
    for(uint32_t i=0; i<numCh; i++)
    {
        //yes i know this is fucking terrible
        //todo: get rid of this dirty ass hack and fix rawimgreader instead
        uint32_t ss;
        ImageFileHeader hTmp;
        imgReader[i].Open(names[i].c_str(), 420, sizeof(ImageFileHeader), &hTmp);
        imgReader[i].Close();
        ss = hTmp.convSettingsUsed.fftSize/2+1;
        imgReader[i].Open(names[i].c_str(), ss, sizeof(ImageFileHeader), &chHdr[i]);
        uint32_t depthFactor = (32/chHdr[i].inputWavHeader.sub1.BitsPerSample);
        chHdr[i].inputWavHeader.sub1.ByteRate *= depthFactor;
        chHdr[i].inputWavHeader.sub1.BlockAlign *= depthFactor;
        chHdr[i].inputWavHeader.sub2.Subchunk2Size *= depthFactor;
        chHdr[i].inputWavHeader.sub1.Subchunk1Size = 16;
        chHdr[i].inputWavHeader.sub1.BitsPerSample = 32;
    }
    for(uint32_t i=0; i<numThreads; i++)
    {
        thr[i].Init(chHdr[0].convSettingsUsed);
    }
    for(uint32_t i=0; i<numCh-1; i++)
    {
        if(memcmp(&chHdr[i], &chHdr[i+1], sizeof(ImageFileHeader)) != 0)
        {
            printf("Header mismatch\n");
            return false;
        }
    }
    if(names.size() != chHdr[0].inputWavHeader.sub1.NumChannels)
    {
        printf("Number of channels does not match number of input files\n");
        return false;
    }
    uint32_t fftSize = chHdr[0].convSettingsUsed.fftSize;
    uint32_t totalSamples = chHdr[0].inputWavHeader.sub2.Subchunk2Size / chHdr[0].inputWavHeader.sub1.BlockAlign;
    uint32_t totalBlocks = (totalSamples+(fftSize/2-1))/(fftSize/2);
    uint32_t samplesToWrite = totalSamples;

    auto underPos = names[0].find("_ch");
    std::string outFileName = names[0].substr(0, underPos) + "_modified.wav";
    CreateCustomSizedFile(outFileName.c_str(), totalSamples*numCh*sizeof(float)+sizeof(wav::Header));
    FILE *outFile = fopen(outFileName.c_str(), "r+b");
    fwrite(&chHdr[0].inputWavHeader, sizeof(wav::Header), 1, outFile);
    for(uint32_t i=1; i<=totalBlocks; i+=2)
    {
        std::vector<float> audInterleaved(fftSize*numCh);
        for(uint32_t ch=0; ch<numCh; ch++)
        {
            std::vector<float> aud(fftSize);
            thr[0].ProcessBlock(&aud[0], &imgReader[ch], i);
            for(uint32_t j=0; j<fftSize; j++)
            {
                audInterleaved[j*numCh+ch] = aud[j];
            }
        }
        uint32_t writeSize = std::min(samplesToWrite, fftSize);
        fwrite(&audInterleaved[0], sizeof(float)*numCh, writeSize, outFile);
        samplesToWrite -= writeSize;
        if(i%512 == 1)
        {
            printf("%d/%d\n",i,totalBlocks);
        }
    }
    printf("samplesToWrite=%d\n",samplesToWrite);
    while(samplesToWrite)
    {
        std::vector<float> nul(262144,0);
        uint32_t writeSize = std::min(samplesToWrite*numCh*sizeof(float), 262144U);
        fwrite(&nul[0],writeSize,1,outFile);
        samplesToWrite -= writeSize;
    }

    for(uint32_t i=0; i<numCh; i++)
    {
        imgReader[i].Close();
    }
    return true;
}
