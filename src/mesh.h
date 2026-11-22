#pragma once

#include <vector>
#include <filesystem>
#include <fstream>
#include <span>
#include <math.h>

// Helpers
std::uint32_t read_u32le(const std::byte* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
float read_f32le(const std::byte* p) noexcept { return std::bit_cast<float>(read_u32le(p)); }

#define __BINARY_HEADER 80
#define __BINARY_COUNT_BYTES 4
#define __BINARY_TRI_BYTES 50 // 12 floats - (normal + 3 verts) + uint16_t

#define ERR_IF_NONFINITE(v, where) \
    if (!std::isfinite(v.fX) || !std::isfinite(v.fY) || !std::isfinite(v.fZ)) { \
        throw StlError(std::string("non-finite vertex coordinate in ") + std::string(where)); \
    }




typedef struct SVec3 {

    double fX, fY, fZ;
} TVec3;


typedef struct STriangle {

    TVec3 V0, V1, V2;
} TTriangle;


enum class STLFormat { 
    
    BIN,
    ASCII 
};


class StlError : public std::runtime_error {

public:

    using std::runtime_error::runtime_error;
};

class CMesh {

private:
    
    STLFormat mFormat = STLFormat::BIN;
    std::vector<TTriangle> vecTriangles;

    void _ParseAscii(std::span<std::byte> data) {

        const std::string text(reinterpret_cast<const char*>(data.data()), data.size());
        const char* p = text.c_str();
        const char* const end = p + text.size();

        auto skip_ws = [&] {

            while (p < end && std::isspace(static_cast<unsigned char>(*p))) ++p;
        };

        auto read_word = [&]() -> std::string_view {

            skip_ws();
            const char* s = p;
            while (p < end && !std::isspace(static_cast<unsigned char>(*p))) ++p;
            return {s, static_cast<std::size_t>(p - s)};
        };

        auto read_double = [&]() -> double {

            skip_ws();
            char* e = nullptr;
            errno = 0;
            const double d = std::strtod(p, &e);
            if (e == p) throw StlError("ASCII STL: expected a number");
            p = e;
            return d;
        };

        if (read_word() != "solid") throw StlError("ASCII STL: missing 'solid' keyword");
        // The solid name may be empty or contain spaces; skip to end of line,
        // unless the file is a one-liner where "facet" follows immediately.
        for (;;) {

            skip_ws();
            const char* save = p;
            const auto w = read_word();
            if (w == "facet" || w == "endsolid" || w.empty()) {
                p = save;
                break;
            }
        }

        for (;;) {

            const auto kw = read_word();
            if (kw.empty()) throw StlError("ASCII STL: unexpected end of file (missing 'endsolid')");
            if (kw == "endsolid") break;
            if (kw != "facet") throw StlError("ASCII STL: expected 'facet' or 'endsolid'");

            if (read_word() != "normal") throw StlError("ASCII STL: expected 'normal'");
            read_double(); read_double(); read_double();

            if (read_word() != "outer" || read_word() != "loop")
                throw StlError("ASCII STL: expected 'outer loop'");

            TTriangle t;

            if (read_word() != "vertex") throw StlError("ASCII STL: expected 'vertex'");
            t.V0.fX = read_double();
            t.V0.fY = read_double();
            t.V0.fZ = read_double();
            ERR_IF_NONFINITE(t.V0, "ASCII STL");
            if (read_word() != "vertex") throw StlError("ASCII STL: expected 'vertex'");
            t.V1.fX = read_double();
            t.V1.fY = read_double();
            t.V1.fZ = read_double();
            ERR_IF_NONFINITE(t.V1, "ASCII STL");
            if (read_word() != "vertex") throw StlError("ASCII STL: expected 'vertex'");
            t.V2.fX = read_double();
            t.V2.fY = read_double();
            t.V2.fZ = read_double();
            ERR_IF_NONFINITE(t.V2, "ASCII STL");

            if (read_word() != "endloop") throw StlError("ASCII STL: expected 'endloop'");
            if (read_word() != "endfacet") throw StlError("ASCII STL: expected 'endfacet'");
            vecTriangles.push_back(t);
        }
    }

    void _ParseBinary(std::span<std::byte> data) {

        std::size_t n = read_u32le(data.data() + __BINARY_HEADER);
        vecTriangles.resize(n);

        std::byte* p = data.data() + __BINARY_HEADER + __BINARY_COUNT_BYTES;
        for (std::size_t i = 0; i < n; ++i, p += __BINARY_TRI_BYTES) {

            // Skip normals
            std::byte* q = p + 12;

            vecTriangles[i].V0.fX = read_f32le(q);
            vecTriangles[i].V0.fY = read_f32le(q + 4);
            vecTriangles[i].V0.fZ = read_f32le(q + 8);
            ERR_IF_NONFINITE(vecTriangles[i].V0, "binary STL");
            q += 12;
            vecTriangles[i].V1.fX = read_f32le(q);
            vecTriangles[i].V1.fY = read_f32le(q + 4);
            vecTriangles[i].V1.fZ = read_f32le(q + 8);
            ERR_IF_NONFINITE(vecTriangles[i].V1, "binary STL");
            q += 12;
            vecTriangles[i].V2.fX = read_f32le(q);
            vecTriangles[i].V2.fY = read_f32le(q + 4);
            vecTriangles[i].V2.fZ = read_f32le(q + 8);
            ERR_IF_NONFINITE(vecTriangles[i].V2, "binary STL");
            q += 12;
        }
    }

    void ParseSTL(std::span<std::byte> data) {

        if (data.empty()) throw StlError("file is empty");

        // Check if binary or ascii
        if (data.size() < __BINARY_HEADER + __BINARY_COUNT_BYTES) mFormat = STLFormat::ASCII;
        else {
            
            const std::uint64_t n = read_u32le(data.data() + __BINARY_HEADER);
            mFormat = (data.size() == __BINARY_HEADER + __BINARY_COUNT_BYTES + n * __BINARY_TRI_BYTES) ? STLFormat::BIN : STLFormat::ASCII;
        }

        if (mFormat == STLFormat::BIN) _ParseBinary(data);
        else {

            // Not a size-consistent binary file. Accept it only if it plausibly is ASCII.
            const std::string_view head(reinterpret_cast<const char*>(data.data()),
                                        data.size() < 5 ? data.size() : 5);

            if (head != "solid") {

                throw StlError(
                    "not a valid STL: not ASCII ('solid') and size does not match the binary "
                    "triangle count (truncated or corrupt?)");
            }
            _ParseAscii(data);
        }
        
        if (vecTriangles.empty()) throw StlError("STL contains no triangles");
    }

public: 

    STLFormat GetFormat() const { return mFormat; }
    const std::vector<TTriangle>* GetTriangles() const { return &vecTriangles; }

    CMesh(std::filesystem::path stl_path) {

        std::error_code err;

        if (!std::filesystem::is_regular_file(stl_path, err))
            throw StlError("can't open '" + stl_path.string() + "': not readable");
        
        std::ifstream in(stl_path, std::ios::binary);
        if (!in) throw StlError("can't open '" + stl_path.string() + "'");

        in.seekg(0, std::ios::end);
        const auto size = in.tellg();
        in.seekg(0, std::ios::beg);
        if (size < 0) throw StlError("can't determine size of '" + stl_path.string() + "'");

        std::vector<std::byte> buf(static_cast<std::size_t>(size));
        if (size > 0 && !in.read(reinterpret_cast<char*>(buf.data()), size))
            throw StlError("error while reading '" + stl_path.string() + "'");

        ParseSTL(buf);
    }
};
