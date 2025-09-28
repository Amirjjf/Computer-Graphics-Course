#include "glad/gl_core_33.h"                // OpenGL

#include <nfd.h>
#include <iostream>
#include <cstdarg>
#include <string>
#include <fmt/core.h>
#include <filesystem>

#include <Eigen/Dense>              // Linear algebra
#include <Eigen/Geometry>

using namespace std;
using namespace Eigen;

#include "Utils.h"
#include <fstream>


void fail(const string& reason)
{
    cerr << reason.c_str();
    exit(1);
}

//------------------------------------------------------------------------

string fileOpenDialog(const string& fileTypeName, const string& fileExtensions)
{
    NFD_Init();

    string retval;

    nfdu8char_t* outPath;
    nfdu8filteritem_t filters[2] = { { fileTypeName.c_str(), fileExtensions.c_str()} };
    nfdopendialogu8args_t args = { 0 };
    args.filterList = filters;
    args.filterCount = 1;
    auto current_directory = filesystem::current_path().generic_string();
    args.defaultPath = current_directory.c_str(); // must do this in two steps! If we don't declare current_directory,
                                                  // the string goes out of scope immediately and the pointer becomes stale.
    nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
    if (result == NFD_OKAY)
    {
        retval = outPath;
        NFD_FreePathU8(outPath);
    }
    else if (result == NFD_CANCEL)
    {
    }
    else
    {
        fail(fmt::format("nativefiledialog Error: {}\n", NFD_GetError()));
    }
    NFD_Quit();
    
    return retval;
}


string loadTextFile(const filesystem::path& source)
{
    auto psf = std::ifstream(source.string(), ios::in);
    auto b = psf.is_open();
    if (!b)
    {
        //cerr << fmt::format("Could not open {}", source.string());
        //cerr << fmt::format("Is your working directory ({}) set correctly?", filesystem::current_path().string());
        //fail("Fatal error, exiting...");
        throw(std::runtime_error(fmt::format("Could not open {}", source.string())));
    }
    psf.seekg(0, ios::end);
    auto l = psf.tellg();
    string ps(l, '\0');
    psf.seekg(0, ios::beg);
    psf.read(ps.data(), ps.size());
    return ps;
}

// Simple XorShift128+ PRNG implementation for obfuscation
namespace {
    struct XorShift128Plus {
        uint64_t s[2];
        explicit XorShift128Plus(uint64_t seed1, uint64_t seed2) { s[0] = seed1 ? seed1 : 0x9e3779b97f4a7c15ULL; s[1] = seed2 ? seed2 : 0xbf58476d1ce4e5b9ULL; }
        uint64_t next() {
            uint64_t x = s[0], y = s[1];
            s[0] = y;
            x ^= x << 23;
            s[1] = x ^ y ^ (x >> 17) ^ (y >> 26);
            return s[1] + y;
        }
    };
}


vector<uint8_t> obfuscate(const vector<uint8_t>& plain, uint64_t seed1, uint64_t seed2)
{
    XorShift128Plus prng(seed1, seed2);
    std::vector<uint8_t> out; out.reserve(plain.size() + 16);
    // Prepend seeds so we can deobfuscate later
    for (int i = 0; i < 8; i++) out.push_back((seed1 >> (i * 8)) & 0xFF);
    for (int i = 0; i < 8; i++) out.push_back((seed2 >> (i * 8)) & 0xFF);
    uint64_t ks = 0; int bytei = 8; // take 8 bytes from each 64-bit block
    for (uint8_t b : plain) {
        if (bytei == 8) { ks = prng.next(); bytei = 0; }
        uint8_t k = (ks >> (bytei * 8)) & 0xFF;
        out.push_back(b ^ k);
        bytei++;
    }
    return out;
}

vector<uint8_t> deobfuscate(const vector<uint8_t>& obfuscated)
{
    // Read seeds from start of obfuscated data
    if (obfuscated.size() < 16) return {};
    uint64_t seed1 = 0, seed2 = 0;

    for (int i = 0; i < 8; i++) seed1 |= (uint64_t)obfuscated[i] << (i * 8);
    for (int i = 0; i < 8; i++) seed2 |= (uint64_t)obfuscated[i + 8] << (i * 8);
    vector<uint8_t> plain; plain.reserve(obfuscated.size() - 16);

    XorShift128Plus prng(seed1, seed2);
    uint64_t ks = 0; int bytei = 8; // take 8 bytes from each 64-bit block

    for (size_t i = 16; i < obfuscated.size(); i++) {
        uint8_t b = obfuscated[i];
        if (bytei == 8) { ks = prng.next(); bytei = 0; }
        uint8_t k = (ks >> (bytei * 8)) & 0xFF;
        plain.push_back(b ^ k);
        bytei++;
    }
    return plain;
}

