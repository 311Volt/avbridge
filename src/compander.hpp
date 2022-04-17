#ifndef AVB_COMPANDER_H
#define AVB_COMPANDER_H

#include "incl/c_cpp.hpp"

#define AVB_COMPANDING_M_SQRT 0x01
#define AVB_COMPANDING_MU_LAW 0x02
#define AVB_COMPANDING_UV_LAW 0x03

namespace avb
{
    typedef std::valarray<uint16_t> (*CompressionFunc16)(std::valarray<float>&, float*);
    typedef std::valarray<float> (*ExpansionFunc16)(std::valarray<uint16_t>&, float*);

    namespace compfn
    {
        std::valarray<uint16_t> F32toUI16(std::valarray<float> v, bool checkRange);

        std::valarray<uint16_t> MuLaw(std::valarray<float>& v, float* param);
        std::valarray<uint16_t> UVLaw(std::valarray<float>& v, float* param);
        std::valarray<uint16_t> MSqrt(std::valarray<float>& v, float* param);
    }

    namespace expnfn
    {
        std::valarray<float> UI16toF32(std::valarray<uint16_t>& v);

        std::valarray<float> MuLaw(std::valarray<uint16_t>& v, float* param);
        std::valarray<float> UVLaw(std::valarray<uint16_t>& v, float* param);
        std::valarray<float> MSqrt(std::valarray<uint16_t>& v, float* param);
    }

    class Compander16
    {
        class Method
        {
        public:
            CompressionFunc16 cmpFn;
            ExpansionFunc16 expFn;
            bool useExpansionLookupTable;
            float paramDefault[2];
            std::valarray<float> expansionLookupTable;
            void CreateExLookupTable(float* param);
        };
        std::map<std::string, uint32_t> methodID;
        std::map<uint32_t, Method> methods;
        Method *cur_method;

        void InitCompandingMethods();

        float param[2];
    public:
        Compander16();
        ~Compander16();

        uint32_t GetMethodID(const char* name);

        void Init(uint32_t method);
        void Init(uint32_t method, float* params);

        std::valarray<uint16_t> Compress(std::valarray<float>& v);
        std::valarray<float> Expand(std::valarray<uint16_t>& v);
    };
}

#endif // AVB_COMPANDER_H
