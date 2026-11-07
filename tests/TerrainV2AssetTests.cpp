#include "SceneFingerprint.h"
#include "terrain/TerrainAsset.h"
#include "terrain/TerrainMetadata.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const std::string &text) {
    ++checks;
    if (!ok)
        throw std::runtime_error(text);
}
uint64_t Integer(const std::vector<uint8_t> &bytes, size_t p, unsigned n) {
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
        v |= uint64_t(bytes.at(p + i)) << (i * 8);
    return v;
}
std::string_view View(const std::vector<uint8_t> &v) {
    return {reinterpret_cast<const char *>(v.data()), v.size()};
}
void File(const std::filesystem::path &p, const std::vector<uint8_t> &bytes) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (!f)
        throw std::runtime_error("fixture file write failed");
}
} // namespace
int main() {
    try {
        auto source = TerrainV2::Create(129, 97, .5f);
        source.bottom = -100;
        source.top = 100;
        for (unsigned z = 0; z < 97; ++z)
            for (unsigned x = 0; x < 129; ++x) {
                size_t i = size_t(z) * 129 + x;
                source.heights[i] = 12 * std::sin(x * .07f) - 7 * std::cos(z * .13f);
                float w = float((x + z) % 17) / 16;
                source.weights[i] = {w, 1 - w, 0, 0};
            }
        source.edits.push_back({TerrainV2::Edit::Kind::Remove, {0, 1, 0}, 2});
        source.layerTextures[0] = "12345678123456781234567812345678";
        const auto original = TerrainV2::Serialize(source);
        std::string error;
        std::vector<uint8_t> bytes, again;
        Check(TerrainV2::EncodeAsset(source, bytes, error), "encode native terrain: " + error);
        Check(TerrainV2::IsAsset(View(bytes)), "native asset is explicitly identified");
        Check(TerrainV2::EncodeAsset(source, again, error) && again == bytes,
              "cooking is byte-for-byte deterministic");
        TerrainV2::Source loaded;
        Check(TerrainV2::DecodeAsset(View(bytes), loaded, error),
              "decode native terrain: " + error);
        Check(TerrainV2::Serialize(loaded) == original,
              "all source values, materials and journal survive exactly");
        const auto before = TerrainV2::Serialize(loaded);
        auto reject = [&](std::vector<uint8_t> bad, const char *message) {
            Check(!TerrainV2::DecodeAsset(View(bad), loaded, error), message);
            Check(TerrainV2::Serialize(loaded) == before,
                  "rejected asset preserves previous source");
        };
        auto bad = bytes;
        bad.back() ^= 1;
        reject(bad, "corrupt sample checksum rejected");
        bad = bytes;
        bad[88] ^= 1;
        reject(bad, "corrupt metadata checksum rejected");
        bad = bytes;
        bad.push_back(0);
        reject(bad, "trailing bytes rejected");
        bad = bytes;
        bad.resize(23);
        reject(bad, "truncated prefix rejected");
        bad = bytes;
        bad.resize(bad.size() - 1);
        reject(bad, "truncated payload rejected");
        bad = bytes;
        bad[20] = 255;
        bad[21] = 255;
        reject(bad, "oversized page count rejected before allocation");
        const size_t index = 88 + Integer(bytes, 12, 4),
                     offset = size_t(Integer(bytes, index + 16, 8)),
                     size = size_t(Integer(bytes, index + 24, 4));
        bad = bytes;
        bad[index] = 64;
        reject(bad, "misplaced page cannot overlap or replace another page");
        bad = bytes;
        bad[offset] = 0;
        bad[offset + 1] = 0;
        bad[offset + 2] = 192;
        bad[offset + 3] = 127; // binary32 quiet NaN
        auto hash = SceneFingerprintSha256(View(bad).substr(offset, size));
        std::copy(hash.begin(), hash.end(), bad.begin() + index + 28);
        reject(bad, "valid checksum cannot make nonfinite physical samples acceptable");
        namespace fs = std::filesystem;
        const auto root =
            fs::temp_directory_path() /
            ("judas-terrain-assets-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
        const auto path = root / "landscape.judasterrain";
        File(path, bytes);
        {
            TerrainV2::AssetPages reader;
            Check(reader.Open(path.string(), error), "open only native header/index: " + error);
            Check(reader.Index().pages.size() == 6 && reader.Index().metadata.heights.empty() &&
                      reader.Index().metadata.weights.empty(),
                  "paged open does not materialize sample arrays");
            TerrainV2::SamplePage page;
            Check(reader.ReadPage(5, page, error) && page.width == 1 && page.height == 33,
                  "bounded edge page retains exact rectangular extent");
            Check(page.heights.front() == source.heights[size_t(64) * 129 + 128],
                  "individual page reads actual requested samples");
            Check(reader.ReadAll(loaded, error) && TerrainV2::Serialize(loaded) == original,
                  "file snapshot reassembles complete source");
            Check(!reader.Open((root / "missing").string(), error) && reader.ReadAll(loaded, error),
                  "failed reopen retains previous opened snapshot");
        }
        auto corrupt = bytes;
        corrupt.back() ^= 1;
        File(path, corrupt);
        {
            TerrainV2::AssetPages reader;
            Check(reader.Open(path.string(), error),
                  "opening an index does not secretly scan unread payload pages");
            TerrainV2::SamplePage page;
            Check(reader.ReadPage(0, page, error), "unaffected page loads independently");
            const auto retained = page.heights;
            Check(!reader.ReadPage(5, page, error) && page.heights == retained,
                  "bad requested page is rejected without replacing valid data");
            Check(!reader.ReadAll(loaded, error) && TerrainV2::Serialize(loaded) == original,
                  "full snapshot never publishes partial page loads");
        }
        File(path, bytes);
        Check(TerrainV2::LoadAsset(path.string(), loaded, error) &&
                  TerrainV2::Serialize(loaded) == original,
              "ordinary native loader works after files close");
        fs::remove_all(root); // readers are closed first, including on Windows
        std::cout << "METRIC native_bytes=" << bytes.size() << " json_bytes=" << original.size()
                  << " page_count=6\n";
        std::cout << "SUMMARY " << checks << " checks 0 failures\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << " after " << checks << " checks\n";
        return 1;
    }
}
