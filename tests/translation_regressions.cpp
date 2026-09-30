#include <kf/platform/translation.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = std::fopen(argv[1], "rb");
    assert(file);
    std::vector<u8> patch;
    for (int c; (c = std::fgetc(file)) != EOF;)
        patch.push_back(static_cast<u8>(c));
    assert(std::fclose(file) == 0);
    kf::ByteBuffer bytes(8);
    std::memcpy(bytes.data(), "abcdefgh", 8);
    kf::AssetTable assets{};
    assert(kf::assets_append(assets, "KF/TEST.", std::move(bytes)));
    const bool ok = kf::translation_apply(assets, patch);
    if (ok)
        std::fwrite(assets[0].bytes.data(), 1, 8, stdout);
    return ok ? 0 : 1;
}
