#include <gtest/gtest.h>
#include "../include/stl/mesh.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct TempFile {
    std::filesystem::path path;
    explicit TempFile(const std::string& filename)
        : path(std::filesystem::temp_directory_path() / filename) {}
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

void AppendFloatLE(std::vector<std::byte>& out, float v) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::byte>((bits >> (8 * i)) & 0xFFu));
    }
}

void AppendTri(std::vector<std::byte>& out,
               std::array<float, 3> a,
               std::array<float, 3> b,
               std::array<float, 3> c) {
    // Normal (ignored by parser)
    AppendFloatLE(out, 0.0f); AppendFloatLE(out, 0.0f); AppendFloatLE(out, 0.0f);
    // 3 vertices
    for (float f : a) AppendFloatLE(out, f);
    for (float f : b) AppendFloatLE(out, f);
    for (float f : c) AppendFloatLE(out, f);
    // 2-byte attribute count
    out.push_back(std::byte{0});
    out.push_back(std::byte{0});
}

std::vector<std::byte> BuildUnitCubeBinaryStl(bool includeTopFace = true) {
    const std::uint32_t triCount = includeTopFace ? 12u : 10u;
    std::vector<std::byte> buf(84, std::byte{0});

    // Put "solid" in binary header intentionally to test format-detection bug resistance
    const char tag[] = "solid binary cube";
    std::memcpy(buf.data(), tag, sizeof(tag) - 1);

    for (int i = 0; i < 4; ++i) {
        buf[80 + i] = static_cast<std::byte>((triCount >> (8 * i)) & 0xFFu);
    }

    // Bottom (z=0)
    AppendTri(buf, {0,0,0}, {1,1,0}, {1,0,0});
    AppendTri(buf, {0,0,0}, {0,1,0}, {1,1,0});
    // Top (z=1)
    if (includeTopFace) {
        AppendTri(buf, {0,0,1}, {1,0,1}, {1,1,1});
        AppendTri(buf, {0,0,1}, {1,1,1}, {0,1,1});
    }
    // Front (y=0)
    AppendTri(buf, {0,0,0}, {1,0,0}, {1,0,1});
    AppendTri(buf, {0,0,0}, {1,0,1}, {0,0,1});
    // Back (y=1)
    AppendTri(buf, {0,1,0}, {1,1,1}, {1,1,0});
    AppendTri(buf, {0,1,0}, {0,1,1}, {1,1,1});
    // Left (x=0)
    AppendTri(buf, {0,0,0}, {0,0,1}, {0,1,1});
    AppendTri(buf, {0,0,0}, {0,1,1}, {0,1,0});
    // Right (x=1)
    AppendTri(buf, {1,0,0}, {1,1,1}, {1,0,1});
    AppendTri(buf, {1,0,0}, {1,1,0}, {1,1,1});

    return buf;
}

void WriteBytes(const std::filesystem::path& p, const std::vector<std::byte>& data) {
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

} // namespace

TEST(StlMeshTest, UnitCubeBinaryStats) {
    TempFile file("test_unit_cube.stl");
    WriteBytes(file.path, BuildUnitCubeBinaryStl(true));

    CMesh mesh(file.path);
    EXPECT_EQ(mesh.GetFormat(), STLFormat::BIN);
    EXPECT_EQ(mesh.GetTriangles()->size(), 12u);

    const auto& bbox = mesh.GetBoundingBox();
    EXPECT_DOUBLE_EQ(bbox.Min.fX, 0.0);
    EXPECT_DOUBLE_EQ(bbox.Max.fX, 1.0);
    EXPECT_DOUBLE_EQ(bbox.Max.fZ, 1.0);

    EXPECT_NEAR(mesh.GetSurfaceArea(), 6.0, 1e-5);
    EXPECT_NEAR(mesh.GetVolume(), 1.0, 1e-5);
    EXPECT_EQ(mesh.GetNumberOfDegenerateTriangles(), 0u);
    EXPECT_TRUE(mesh.IsWaterTight());
}

TEST(StlMeshTest, AsciiStlParsesCleanly) {
    TempFile file("test_ascii_tri.stl");
    const std::string ascii =
        "solid single_tri\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 2 0 0\n"
        "      vertex 0 3 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid single_tri\n";

    std::ofstream out(file.path, std::ios::binary);
    out << ascii;
    out.close();

    CMesh mesh(file.path);
    EXPECT_EQ(mesh.GetFormat(), STLFormat::ASCII);
    EXPECT_EQ(mesh.GetTriangles()->size(), 1u);
    EXPECT_NEAR(mesh.GetSurfaceArea(), 3.0, 1e-5); // 0.5 * 2 * 3 = 3.0
    EXPECT_FALSE(mesh.IsWaterTight());
}

TEST(StlMeshTest, TruncatedBinaryThrowsStlError) {
    TempFile file("test_corrupt.stl");
    auto data = BuildUnitCubeBinaryStl(true);
    data.resize(120); // Truncate mid-triangle
    WriteBytes(file.path, data);

    EXPECT_THROW(CMesh{file.path}, StlError);
}

TEST(StlMeshTest, MissingFileThrowsStlError) {
    EXPECT_THROW(CMesh{"/tmp/definitely_missing_12345.stl"}, StlError);
}