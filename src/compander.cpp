#include "compander.hpp"

std::valarray<uint16_t> avb::compfn::F32toUI16(std::valarray<float> v, bool checkRange)
{
    std::valarray<uint16_t> r(v.size());
    v *= 65535.0f;
    if(checkRange)
    {
        for(auto& i : v)
            i = std::max(i, 0.0f);
        for(auto& i : v)
            i = std::min(i, 65535.0f);
    }
    for(uint32_t i=0; i<v.size(); i++)
        r[i] = v[i];
    return r;
}
std::valarray<float> avb::expnfn::UI16toF32(std::valarray<uint16_t>& v)
{
    std::valarray<float> r(v.size());
    for(uint32_t i=0; i<v.size(); i++)
        r[i] = (float)v[i] / 65535.0f;
    return r;
}


std::valarray<uint16_t> avb::compfn::MuLaw(std::valarray<float>& v, float* param)
{
    std::valarray<float> rf(1.0f / std::log(1.0f+param[0]), v.size());
    rf *= std::log(1.0f+param[0]*v);
    return F32toUI16(rf, true);
}
std::valarray<uint16_t> avb::compfn::UVLaw(std::valarray<float>& v, float* param)
{
    float gamma = (param[0]*param[1]-1.0f) / (param[0]+param[1]-2.0f);
    float beta = 1.0f / (1.0f - gamma);
    float alpha = 1.0f / (param[0]-gamma);
    std::valarray<float> rf = v / ((beta-alpha)*v + alpha) + gamma*v;
    return F32toUI16(rf, true);
}
std::valarray<uint16_t> avb::compfn::MSqrt(std::valarray<float>& v, float* param)
{
    std::valarray<float> rf = std::sqrt(v);
    int numIter = int(param[0])-1;
    for(int i=0; i<numIter; i++)
        rf = std::sqrt(rf);
    return F32toUI16(rf, true);
}

std::valarray<float> avb::expnfn::MuLaw(std::valarray<uint16_t>& v, float* param)
{
    std::valarray<float> rf(1.0f / param[0], v.size());
    std::valarray<float> vf = UI16toF32(v);
    for(uint32_t i=0; i<v.size(); i++)
        rf[i] *= std::pow(1.0f + param[0], vf[i]) - 1.0f;
    return rf;
}
std::valarray<float> avb::expnfn::UVLaw(std::valarray<uint16_t>& v, float* param)
{
    std::valarray<float> vf = UI16toF32(v);
    float gamma = (param[0]*param[1]-1.0f) / (param[0]+param[1]-2.0f);
    float beta = 1.0f / (1.0f - gamma);
    float alpha = 1.0f / (param[0]-gamma);

    float a = gamma*(alpha-beta);
    std::valarray<float> b = vf*(beta-alpha) - gamma*alpha - 1.0f;
    std::valarray<float> c = vf*alpha;

    return (-b-std::sqrt(b*b - 4.0f*a*c)) / (2.0f*a);
}
std::valarray<float> avb::expnfn::MSqrt(std::valarray<uint16_t>& v, float* param)
{
    std::valarray<float> vf = UI16toF32(v);
    vf *= vf;
    int numIter = int(param[0])-1;
    for(int i=0; i<numIter; i++)
        vf *= vf;
    return vf;
}


void avb::Compander16::Method::CreateExLookupTable(float* param)
{
    std::valarray<uint16_t> v(65536);
    for(uint32_t i=0; i<65536; i++)
        v[i] = i;
    expansionLookupTable = (*expFn)(v, param);
}

avb::Compander16::Compander16()
{
    cur_method = nullptr;
    InitCompandingMethods();
}
avb::Compander16::~Compander16()
{

}


void avb::Compander16::InitCompandingMethods()
{
    Method m;

    m.cmpFn = compfn::MuLaw;
    m.expFn = expnfn::MuLaw;
    m.paramDefault[0] = 64.0f;
    m.paramDefault[1] = 0.0f;
    m.useExpansionLookupTable = true;
    methods[AVB_COMPANDING_MU_LAW] = m;
    methodID["mulaw"] = AVB_COMPANDING_MU_LAW;

    m.cmpFn = compfn::UVLaw;
    m.expFn = expnfn::UVLaw;
    m.paramDefault[0] = 256.0f;
    m.paramDefault[1] = 0.25f;
    m.useExpansionLookupTable = true;
    methods[AVB_COMPANDING_UV_LAW] = m;
    methodID["uvlaw"] = AVB_COMPANDING_UV_LAW;

    m.cmpFn = compfn::MSqrt;
    m.expFn = expnfn::MSqrt;
    m.paramDefault[0] = 1.0f;
    m.paramDefault[1] = 0.0f;
    m.useExpansionLookupTable = false;
    methods[AVB_COMPANDING_M_SQRT] = m;
    methodID["sqrt"] = AVB_COMPANDING_M_SQRT;
}

uint32_t avb::Compander16::GetMethodID(const char* name)
{
    std::string name_s(name);
    if(methodID.count(name_s) == 0)
        return 0;
    return methodID[name_s];
}

void avb::Compander16::Init(uint32_t method)
{
    Method& meth = methods[method];
    Init(method, meth.paramDefault);
}

void avb::Compander16::Init(uint32_t method, float* params)
{
    if(methods.count(method) == 0)
        printf("ERROR: no method with id %d found\n", method);
    Method& meth = methods[method];
    if(meth.useExpansionLookupTable)
        meth.CreateExLookupTable(params);
    param[0] = params[0];
    param[1] = params[1];
    cur_method = &meth;
}


std::valarray<uint16_t> avb::Compander16::Compress(std::valarray<float>& v)
{
    return (*(cur_method->cmpFn))(v, param);
}
std::valarray<float> avb::Compander16::Expand(std::valarray<uint16_t>& v)
{
    if(!cur_method->useExpansionLookupTable)
         return (*(cur_method->expFn))(v, param);
    std::valarray<float> r(v.size());
    for(uint32_t i=0; i<v.size(); i++)
        r[i] = cur_method->expansionLookupTable[v[i]];
    return r;
}
