#ifndef AVB_CONVERTER_H
#define AVB_CONVERTER_H

#include "incl/c_cpp.hpp"
#include "fftw3/fftw3.h"
#include "compander.hpp"
#include "windowing.hpp"
#include "fileio.hpp"

namespace avb
{
    struct ConverterSettings
    {
        uint32_t fftSize;
        uint32_t compandingMethod;
        uint32_t windowFunction;
        float companderParam[2];
        char horizontalTime;
        char forceOverwrite;
        char outputFloat32Audio;
    };
    struct ImageFileHeader
    {
        uint32_t magicNumber;
        uint32_t headerVersion;
        uint32_t headerSize;
        ConverterSettings convSettingsUsed;
        wav::Header inputWavHeader;
    };
    ImageFileHeader MakeBlankImageFileHeader();
    ConverterSettings MakeDefaultConverterSettings();

    class ForwardConverterThread
    {
        bool fftwPlanExists;
        fftwf_plan fftwPlan;
        float *fftwAudioBuffer;
        fftwf_complex *fftwDFTBuffer;
        std::valarray<float> window;
        std::valarray<float> outTmpReal, outTmpImag, outTmpMagn;
        std::valarray<uint16_t> outTmpReal16, outTmpImag16, outTmpMagn16;
        ConverterSettings settings;
        Compander16 compressor;
        std::thread stlThread;
    public:
        ForwardConverterThread();
        ForwardConverterThread(ConverterSettings t_settings);
        ~ForwardConverterThread();

        std::vector<std::valarray<float>> inputs, inputsNext;
        std::vector<Pixel16> outputs, outputsLast;

        bool Init(ConverterSettings t_settings);
        void Deinit();
        void Destroy();
        void ProcessBlock(Pixel16* output, std::valarray<float>& input);
        void Process();
        void ProcessAsync();
        void WaitForCompletion();

    };

    class ForwardConverter
    {
        std::vector<ForwardConverterThread> thr;
        std::vector<ImageFileHeader> chHdr;
        WavReader audioReader;
        ConverterSettings settings;
        //RawImgWriter imgWriter;
        int numThreads;
        bool isNumber7smooth(uint32_t n);
        std::string RemoveFilenameExtension(std::string s);
    public:
        ForwardConverter();
        ~ForwardConverter();

        bool Init(ConverterSettings t_settings);
        bool Convert(const char* inputFilename);
        void Destroy();
    };

    class BackwardConverterThread
    {
        std::thread stlThread;
        bool fftwPlanExists;
        fftwf_plan fftwPlan;
        float *fftwAudioBuffer;
        fftwf_complex *fftwDFTBuffer;
        ConverterSettings settings;
        Compander16 expander;
        ImageFileHeader inputHdr;
        
        std::valarray<float> window, inverseSquareWindow;
        
    public:
        void ProcessBlock(float* out, RawImgReader16* reader, int32_t centerBlockPos);
        BackwardConverterThread();
        BackwardConverterThread(ConverterSettings t_settings);
        ~BackwardConverterThread();

        bool Init(ConverterSettings t_settings);
        void Deinit();
    };
    
    std::vector<std::string> FindMatchingFilenamesBC(const char* name);
    class BackwardConverter
    {
        std::vector<RawImgReader16> imgReader;
        std::vector<BackwardConverterThread> thr;
        uint32_t numThreads, numCh;
    public:
        bool Convert(const char* name);
    };

}

#endif // AVB_CONVERTER_H
