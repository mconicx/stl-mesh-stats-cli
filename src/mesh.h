#pragma once

#include <vector>
#include <filesystem>
#include <fstream>
#include <span>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <bit>
#include <algorithm>
#include <utility>
#include <functional>
#include <hash_map>

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

#define DEGEN_AREA_EPS 1e-7


typedef struct SVec3 {

    double fX = 0.;
    double fY = 0.;
    double fZ = 0.;

    SVec3() = default;
    explicit SVec3(double value) : fX(value), fY(value), fZ(value) {}
    SVec3(double x, double y, double z) : fX(x), fY(y), fZ(z) {}
} TVec3;

typedef struct STriangle {

    TVec3 V0, V1, V2;
} TTriangle;

typedef struct SBoundingBox {

    TVec3 Min, Max;
} TBoundingBox;

typedef struct  SVertexKey {

    std::int64_t x, y, z;
    bool operator==(const SVertexKey&) const = default;
} TVertexKey;

typedef struct SVertexKeyHash {

    std::size_t operator()(const TVertexKey& k) const noexcept {

        std::size_t h1 = std::hash<std::int64_t>{}(k.x);
        std::size_t h2 = std::hash<std::int64_t>{}(k.y);
        std::size_t h3 = std::hash<std::int64_t>{}(k.z);
        std::size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
} TVertexKeyHash;

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
    TBoundingBox mBBox;
    double dSurfaceArea;
    unsigned long iDegenerateTris;
    std::unordered_map<std::uint64_t, std::uint32_t> edgeCounts;
    double dVolume;
    unsigned long iBoundaryEdges;
    unsigned long iNonManifoldEdges;
    bool bWatertight;

    static std::uint64_t makeEdgeKey(std::uint32_t id0, std::uint32_t id1) noexcept {

        if (id1 < id0) std::swap(id0, id1);
        return (static_cast<std::uint64_t>(id0) << 32) | id1;
    }

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

        // Zero out members
        vecTriangles.clear();
        mBBox.Min = TVec3(std::numeric_limits<float>::max());
        mBBox.Max = TVec3(std::numeric_limits<float>::lowest());
        dSurfaceArea = 0.;
        iDegenerateTris = 0;
        dVolume = 0.;
        iBoundaryEdges = 0;
        iNonManifoldEdges = 0;
        bWatertight = false;

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
        
        // Check if we have any triangles
        if (vecTriangles.empty()) throw StlError("STL contains no triangles");

        // Calculate data
        TVec3 vOrigin = vecTriangles[0].V0;
        double dSignedVolumeSum = 0.;

        std::unordered_map<TVertexKey, std::uint32_t, TVertexKeyHash> vertexToId;
        vertexToId.reserve(vecTriangles.size());
        auto getVertexId = [&](const TVec3& v) -> std::uint32_t {

            constexpr double kQuantizeScale = 1e5; // 0.00001 mm grid
            const TVertexKey key{

                static_cast<std::int64_t>(std::llround(v.fX * kQuantizeScale)),
                static_cast<std::int64_t>(std::llround(v.fY * kQuantizeScale)),
                static_cast<std::int64_t>(std::llround(v.fZ * kQuantizeScale))
            };
            auto [it, inserted] = vertexToId.emplace(key, static_cast<std::uint32_t>(vertexToId.size()));
            return it->second;
        };

        for (const auto& t : vecTriangles) {

            // Bounding box
            mBBox.Min = TVec3(std::min(mBBox.Min.fX, std::min(t.V0.fX, std::min(t.V1.fX, t.V2.fX))),
                              std::min(mBBox.Min.fY, std::min(t.V0.fY, std::min(t.V1.fY, t.V2.fY))),
                              std::min(mBBox.Min.fZ, std::min(t.V0.fZ, std::min(t.V1.fZ, t.V2.fZ))));
            mBBox.Max = TVec3(std::max(mBBox.Max.fX, std::max(t.V0.fX, std::max(t.V1.fX, t.V2.fX))),
                              std::max(mBBox.Max.fY, std::max(t.V0.fY, std::max(t.V1.fY, t.V2.fY))),
                              std::max(mBBox.Max.fZ, std::max(t.V0.fZ, std::max(t.V1.fZ, t.V2.fZ))));

            // Shifted coordinates for numerical stability on Volume
            const double v0x = t.V0.fX - vOrigin.fX, v0y = t.V0.fY - vOrigin.fY, v0z = t.V0.fZ - vOrigin.fZ;
            const double v1x = t.V1.fX - vOrigin.fX, v1y = t.V1.fY - vOrigin.fY, v1z = t.V1.fZ - vOrigin.fZ;
            const double v2x = t.V2.fX - vOrigin.fX, v2y = t.V2.fY - vOrigin.fY, v2z = t.V2.fZ - vOrigin.fZ;

            // Surface area & degenerate triangle check: 0.5 * |(V1 - V0) x (V2 - V0)|
            const double e1x = v1x - v0x, e1y = v1y - v0y, e1z = v1z - v0z;
            const double e2x = v2x - v0x, e2y = v2y - v0y, e2z = v2z - v0z;
            const double cx = e1y * e2z - e1z * e2y;
            const double cy = e1z * e2x - e1x * e2z;
            const double cz = e1x * e2y - e1y * e2x;
            const double triArea = 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);

            if (triArea < DEGEN_AREA_EPS) ++iDegenerateTris;

            dSurfaceArea += triArea;

            // Signed volume contribution: dot(V0, V1 x V2) / 6.0
            const double vx = v1y * v2z - v1z * v2y;
            const double vy = v1z * v2x - v1x * v2z;
            const double vz = v1x * v2y - v1y * v2x;
            dSignedVolumeSum += (v0x * vx + v0y * vy + v0z * vz) / 6.0;

            // Topology / edge counting
            const std::uint32_t id0 = getVertexId(t.V0);
            const std::uint32_t id1 = getVertexId(t.V1);
            const std::uint32_t id2 = getVertexId(t.V2);

            if (id0 != id1 && id1 != id2 && id2 != id0) {

                ++edgeCounts[makeEdgeKey(id0, id1)];
                ++edgeCounts[makeEdgeKey(id1, id2)];
                ++edgeCounts[makeEdgeKey(id2, id0)];
            }
        }

        dVolume = std::abs(dSignedVolumeSum);

        for (const auto& [edgeKey, count] : edgeCounts) {

            if (count == 1) iBoundaryEdges++;
            else if (count > 2) iNonManifoldEdges++;
        }

        bWatertight = (iBoundaryEdges == 0 && iNonManifoldEdges == 0);
    }

public: 

    STLFormat GetFormat() const { return mFormat; }
    const std::vector<TTriangle>* GetTriangles() const { return &vecTriangles; }
    const TBoundingBox& GetBoundingBox() const { return mBBox; }
    const double GetSurfaceArea() const { return dSurfaceArea; }
    const double GetVolume() const { return dVolume; }
    const unsigned long GetNumberOfDegenerateTriangles() const { return iDegenerateTris; }
    const bool IsWaterTight() const { return bWatertight; }

    CMesh(std::filesystem::path stl_path) {

        std::error_code err;

        if (!std::filesystem::is_regular_file(stl_path, err))
            throw StlError("can't open '" + stl_path.string() + "': not readable");
        
        std::ifstream in(stl_path, std::ios::binary);
        if (!in) throw StlError("can't open '" + stl_path.string() + "'");

        in.seekg(0, std::ios::end);
        const auto size = in.tellg();
        in.seekg(0, std::ios::beg);
        if (size < 0) throw StlError("file is empty or program couldn't determine size of '" + stl_path.string() + "'");

        std::vector<std::byte> buf(static_cast<std::size_t>(size));
        if (size > 0 && !in.read(reinterpret_cast<char*>(buf.data()), size))
            throw StlError("error while reading '" + stl_path.string() + "'");

        ParseSTL(buf);
    }
};
