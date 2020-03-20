/*
    This is part of libswirl
*/
#include <license/bsd>

#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include "libswirl/hw/aica/dsp_backend.h"

using namespace std;

map<string, int> getDefaultInst()
{
    map<string, int> rv;

    rv["TRA"] = 0;
    rv["TWT"] = 0;
    rv["TWA"] = 0;
    rv["XSEL"] = 0;
    rv["YSEL"] = 0;
    rv["IRA"] = 0;
    rv["IWT"] = 0;
    rv["IWA"] = 0;
    rv["EWT"] = 0;
    rv["EWA"] = 0;
    rv["ADRL"] = 0;
    rv["FRCL"] = 0;
    rv["SHIFT"] = 0;
    rv["YRL"] = 0;
    rv["NEGB"] = 0;
    rv["ZERO"] = 0;
    rv["BSEL"] = 0;
    rv["NOFL"] = 0;
    rv["TABLE"] = 0;
    rv["MWT"] = 0;
    rv["MRD"] = 0;
    rv["MASA"] = 0;
    rv["ADREB"] = 0;
    rv["NXADR"] = 0;

    return rv;
}

map<string, int> decodeInst(u32 *Inst)
{
    _INST i = {0};

    DSPBackend::DecodeInst(Inst, &i);

    map<string, int> rv;

    rv["TRA"] = i.TRA;
    rv["TWT"] = i.TWT;
    rv["TWA"] = i.TWA;
    rv["XSEL"] = i.XSEL;
    rv["YSEL"] = i.YSEL;
    rv["IRA"] = i.IRA;
    rv["IWT"] = i.IWT;
    rv["IWA"] = i.IWA;
    rv["EWT"] = i.EWT;
    rv["EWA"] = i.EWA;
    rv["ADRL"] = i.ADRL;
    rv["FRCL"] = i.FRCL;
    rv["SHIFT"] = i.SHIFT;
    rv["YRL"] = i.YRL;
    rv["NEGB"] = i.NEGB;
    rv["ZERO"] = i.ZERO;
    rv["BSEL"] = i.BSEL;
    rv["NOFL"] = i.NOFL;
    rv["TABLE"] = i.TABLE;
    rv["MWT"] = i.MWT;
    rv["MRD"] = i.MRD;
    rv["MASA"] = i.MASA;
    rv["ADREB"] = i.ADREB;
    rv["NXADR"] = i.NXADR;

    return rv;
}

void encodeInst(u32 *Inst, map<string, int> &desc)
{
    _INST i = {0};

    i.TRA = desc["TRA"];
    i.TWT = desc["TWT"];
    i.TWA = desc["TWA"];
    i.XSEL = desc["XSEL"];
    i.YSEL = desc["YSEL"];
    i.IRA = desc["IRA"];
    i.IWT = desc["IWT"];
    i.IWA = desc["IWA"];
    i.EWT = desc["EWT"];
    i.EWA = desc["EWA"];
    i.ADRL = desc["ADRL"];
    i.FRCL = desc["FRCL"];
    i.SHIFT = desc["SHIFT"];
    i.YRL = desc["YRL"];
    i.NEGB = desc["NEGB"];
    i.ZERO = desc["ZERO"];
    i.BSEL = desc["BSEL"];
    i.NOFL = desc["NOFL"];
    i.TABLE = desc["TABLE"];
    i.MWT = desc["MWT"];
    i.MRD = desc["MRD"];
    i.MASA = desc["MASA"];
    i.ADREB = desc["ADREB"];
    i.NXADR = desc["NXADR"];

    DSPBackend::EncodeInst(Inst, &i);
}

string DisassembleDesc(map<string, int> &desc)
{
    stringstream rv;

    rv << "TRA:" << desc["TRA"] << " ";
    rv << "TWT:" << desc["TWT"] << " ";
    rv << "TWA:" << desc["TWA"] << " ";
    rv << "XSEL:" << desc["XSEL"] << " ";
    rv << "YSEL:" << desc["YSEL"] << " ";
    rv << "IRA:" << desc["IRA"] << " ";
    rv << "IWT:" << desc["IWT"] << " ";
    rv << "IWA:" << desc["IWA"] << " ";
    rv << "EWT:" << desc["EWT"] << " ";
    rv << "EWA:" << desc["EWA"] << " ";
    rv << "ADRL:" << desc["ADRL"] << " ";
    rv << "FRCL:" << desc["FRCL"] << " ";
    rv << "SHIFT:" << desc["SHIFT"] << " ";
    rv << "YRL:" << desc["YRL"] << " ";
    rv << "NEGB:" << desc["NEGB"] << " ";
    rv << "ZERO:" << desc["ZERO"] << " ";
    rv << "BSEL:" << desc["BSEL"] << " ";
    rv << "NOFL:" << desc["NOFL"] << " ";
    rv << "TABLE:" << desc["TABLE"] << " ";
    rv << "MWT:" << desc["MWT"] << " ";
    rv << "MRD:" << desc["MRD"] << " ";
    rv << "MASA:" << desc["MASA"] << " ";
    rv << "ADREB:" << desc["ADREB"] << " ";
    rv << "NXADR:" << desc["NXADR"];

    return rv.str();
}

map<string, int> AssembleDesc(string text)
{
    map<string, int> rv = getDefaultInst();
    istringstream line(text);

    string word;
    while (line >> word)
    {
        istringstream wordstream(word);
        string decl, value;
        if (!getline(wordstream, decl, ':'))
        {
            printf("Invalid asm %s\n", text.c_str());
            exit(-4);
        }
        if (!(wordstream >> value))
        {
            printf("Invalid asm %s\n", text.c_str());
            exit(-5);
        }

        rv[decl] = atoi(value.c_str());
    }

    return rv;
}

int main(int argc, char **argv)
{

    if (argc != 2 && argc != 3)
    {
        printf("expected %s src_file [-d]\n", argv[0]);
        return -1;
    }

    uint32_t mpro[128 * 4] = {0};

    if (argc == 2)
    {
        const string inputFile = argv[1];

        ifstream infile(inputFile, std::ios_base::binary);

        std::string line;
        int op = 0;
        while (getline(infile, line))
        {
            if (line.size() == 0 || line[0] == '#')
                continue;
            if (op == 128)
            {
                printf("more than 128 ops\n");
                exit(-5);
            }

            auto desc = AssembleDesc(line);
            encodeInst(&mpro[op * 4], desc);
            op++;
        }

        freopen(NULL, "wb", stdout);
        fwrite(mpro, 4, 128 * 4, stdout);
    }
    else
    {
        FILE *f = fopen(argv[1], "rb");
        if (!f)
        {
            printf("failed to open %s\n", argv[1]);
            return -2;
        }
        if (fread(mpro, 4, 128 * 4, f) != 128 * 4)
        {
            printf("file %s is not the right size\n", argv[1]);
            return -3;
        }
        fclose(f);

        for (int i = 0; i < 128; i++)
        {
            auto inst = decodeInst(&mpro[i * 4]);

            auto desc = decodeInst(&mpro[i * 4]);
            auto text = DisassembleDesc(desc);
            printf("#step %d\n", i);
            puts(text.c_str());
        }
    }
}
