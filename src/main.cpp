#include "incl/c_cpp.hpp"
#include "converter.hpp"
#include "fileio.hpp"

std::valarray<float> sinewave(uint32_t len, float freq, float phase, float amplitude)
{
    std::valarray<float> r(len);
    for(uint32_t i=0; i<len; i++)
    {
        double x = i;
        r[i] = amplitude*std::sin(2.0*3.141592653589793*freq*x-phase);
    }
    return r;
}

void print_valarray(std::valarray<float> v)
{
    for(auto& i : v)
        printf("%f\t",i);
}

int main(int argc, char** argv)
{
    bool backward = false;
    if(backward)
    {
        avb::BackwardConverter cnvB;
        cnvB.Convert("unbrkn_edit");
    }
    else
    {
        avb::ForwardConverter cnv;
        cnv.Init(avb::MakeDefaultConverterSettings());

        bool conv_succ = cnv.Convert(argv[1]);
        if(!conv_succ)
        {
            puts("Conversion was aborted due to an error.");
        }
    }
    /*avb::ForwardConverterThread testthr;
    testthr.Init(avb::MakeDefaultConverterSettings());

    int blocks1024 = 0;
    std::ifstream testfile("unbrkn.wav", std::ios::binary);
    std::ofstream outfile("unbrkn.raw", std::ios::binary);
    testfile.seekg(44);
    std::valarray<float> block(0.0f, 2048);
    while(1)
    {
        float dat[2048];
        testfile.read((char*)dat, 2048*sizeof(float));
        if(testfile.eof())
            break;
        block = block.cshift(1024);
        for(int i=0; i<1024; i++)
            block[i+1024] = dat[2*i+1];
        std::vector<std::valarray<float>> xd(1);
        xd[0] = block;
        testthr.inputs = xd;
        testthr.Process();

        outfile.write((char*)&testthr.outputs[0][0], testthr.outputs[0].size()*sizeof(avb::Pixel16));

        blocks1024++;

        if(blocks1024 % 256 == 0)
        {
            int seconds = (blocks1024*1024) / 44100;
            //printf("block %d (timestamp: %d:%02d)\n", blocks1024, seconds/60, seconds%60);
        }
    }

    printf("Resulting image is 1025x%d", blocks1024);
*/
    return 0;
}
