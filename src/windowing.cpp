#include "windowing.hpp"

#define PI 3.141592653589793


std::map<uint32_t, avb::WindowFunctionPtr> avb::window::fnPointer;
std::map<std::string, uint32_t> avb::window::id;

double avb::window::Rectangular(double x)
{
    return 1.0;
}

double avb::window::Triangular(double x)
{
    return 1.0 - std::abs(2.0*x-1.0);
}

double avb::window::Hann(double x)
{
    double r = std::sin(PI*x);
    return r*r;
}

double avb::window::Hamming(double x)
{
    double a0 = 25.0 / 46.0;
    return a0 - (1.0 - a0)*std::cos(2.0*PI*x);
}

double avb::window::Blackman(double x)
{
    return 0.42 - 0.5*std::cos(2.0*PI*x) + 0.08*std::cos(4.0*PI*x);
}

double avb::window::BlackmanHarris(double x)
{
    double r = 0.35875;
    r -= 0.48829 * std::cos(2.0*PI*x);
    r += 0.14128 * std::cos(4.0*PI*x);
    r -= 0.01168 * std::cos(6.0*PI*x);
    return r;
}

void avb::window::InitMaps()
{
    fnPointer[AVB_WINDOW_RECTANGULAR] = Rectangular;
    fnPointer[AVB_WINDOW_TRIANGULAR] = Triangular;
    fnPointer[AVB_WINDOW_HANN] = Hann;
    fnPointer[AVB_WINDOW_HAMMING] = Hamming;
    fnPointer[AVB_WINDOW_BLACKMAN] = Blackman;
    fnPointer[AVB_WINDOW_BLACKMAN_HARRIS] = BlackmanHarris;

    id["none"] = AVB_WINDOW_RECTANGULAR;
    id["rectangular"] = AVB_WINDOW_RECTANGULAR;
    id["triangular"] = AVB_WINDOW_TRIANGULAR;
    id["hann"] = AVB_WINDOW_HANN;
    id["hamming"] = AVB_WINDOW_HAMMING;
    id["blackman"] = AVB_WINDOW_BLACKMAN;
    id["blackmanharris"] = AVB_WINDOW_BLACKMAN_HARRIS;
}

avb::WindowFunctionPtr avb::GetWindowFunctionPtr(uint32_t id)
{
    if(!window::fnPointer.size())
        window::InitMaps();
    if(!window::fnPointer.count(id))
        id = AVB_WINDOW_RECTANGULAR;
    return window::fnPointer[id];
}
avb::WindowFunctionPtr avb::GetWindowFunctionPtr(std::string name)
{
    if(!window::id.size())
        window::InitMaps();
    if(!window::id.count(name))
        name = "none";
    return GetWindowFunctionPtr(window::id[name]);
}

std::valarray<float> avb::MakeWindow(uint32_t sz, uint32_t id)
{
    return MakeWindow(sz, GetWindowFunctionPtr(id));
}
std::valarray<float> avb::MakeWindow(uint32_t sz, std::string name)
{
    return MakeWindow(sz, GetWindowFunctionPtr(name));
}
std::valarray<float> avb::MakeWindow(uint32_t sz, WindowFunctionPtr fn)
{
    std::valarray<float> r(sz);
    for(uint32_t i=0; i<sz; i++)
        r[i] = (*fn)((double)i / (double)(sz-1));
    return r;
}
