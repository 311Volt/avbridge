#ifndef AVB_WINDOWING_H
#define AVB_WINDOWING_H

#include "incl/c_cpp.hpp"

#define AVB_WINDOW_RECTANGULAR 0x00
#define AVB_WINDOW_TRIANGULAR 0x01
#define AVB_WINDOW_HANN 0x02
#define AVB_WINDOW_HAMMING 0x03
#define AVB_WINDOW_BLACKMAN 0x04
#define AVB_WINDOW_BLACKMAN_HARRIS 0x05


namespace avb
{
    typedef double(*WindowFunctionPtr)(double);
    namespace window
    {
        double Rectangular(double x);
        double Triangular(double x);
        double Hann(double x);
        double Hamming(double x);
        double Blackman(double x);
        double BlackmanHarris(double x);

        extern std::map<uint32_t, WindowFunctionPtr> fnPointer;
        extern std::map<std::string, uint32_t> id;
        void InitMaps();
    }

    WindowFunctionPtr GetWindowFunctionPtr(uint32_t id);
    WindowFunctionPtr GetWindowFunctionPtr(std::string name);

    std::valarray<float> MakeWindow(uint32_t sz, uint32_t id);
    std::valarray<float> MakeWindow(uint32_t sz, std::string name);
    std::valarray<float> MakeWindow(uint32_t sz, WindowFunctionPtr fn);
}

#endif // AVB_WINDOWING_H
