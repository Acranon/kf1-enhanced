#include <kf/platform/disc.hpp>
#include <kf/platform/assets.hpp>
#include <kf/platform/disc_layout.hpp>
#include <string>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#ifdef _WIN32
#include <filesystem>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace kf {
static constexpr unsigned cue_line_capacity = 1024;
static constexpr unsigned disc_path_capacity = 4096;
static constexpr std::size_t cue_file_capacity = 65536;
static constexpr unsigned ascii_first_printable = 32;
bool disc_cue_image(const char *text, char *filename, std::size_t capacity) {
    bool file = false, track = false, index = false;
    while (*text) {
        const char *end = std::strchr(text, '\n');
        if (!end)
            end = text + std::strlen(text);
        while (text < end && std::isspace(static_cast<unsigned char>(*text)))
            ++text;
        const char *tail = end;
        while (tail > text && std::isspace(static_cast<unsigned char>(tail[-1])))
            --tail;
        char line[cue_line_capacity];
        const auto length = static_cast<std::size_t>(tail - text);
        if (length >= sizeof line)
            return false;
        std::memcpy(line, text, length);
        line[length] = 0;
        text = *end ? end + 1 : end;
        if (!length || std::strncmp(line, "REM ", 4) == 0)
            continue;
        if (std::strncmp(line, "FILE ", 5) == 0) {
            if (file || track || index)
                return false;
            const char *name = line + 5;
            while (*name == ' ' || *name == '\t')
                ++name;
            const bool quoted = *name == '"';
            if (quoted)
                ++name;
            const char *name_end = quoted ? std::strchr(name, '"') : std::strpbrk(name, " \t");
            if (!name_end || name_end == name)
                return false;
            const auto name_size = static_cast<std::size_t>(name_end - name);
            const char *kind = name_end + (quoted ? 1 : 0);
            if (*kind != ' ' && *kind != '\t')
                return false;
            while (*kind == ' ' || *kind == '\t')
                ++kind;
            if (std::strcmp(kind, "BINARY") != 0 || name_size >= capacity)
                return false;
            // Only a sibling file: never resolve absolute paths or traversal.
            for (const char *at = name; at != name_end; ++at)
                if (*at == '/' || *at == '\\' || *at == ':' ||
                    static_cast<unsigned char>(*at) < ascii_first_printable)
                    return false;
            std::memcpy(filename, name, name_size);
            filename[name_size] = 0;
            if (std::strcmp(filename, ".") == 0 || std::strcmp(filename, "..") == 0)
                return false;
            file = true;
        } else if (std::strcmp(line, "TRACK 01 MODE2/2352") == 0) {
            if (!file || track || index)
                return false;
            track = true;
        } else if (std::strcmp(line, "INDEX 01 00:00:00") == 0) {
            if (!track || index)
                return false;
            index = true;
        } else {
            return false;
        }
    }
    return file && track && index;
}

#ifdef _WIN32
static FILE *open_image(const char *path, std::size_t *size) {
    std::error_code error;
    const std::filesystem::path file_path = std::filesystem::path(reinterpret_cast<const char8_t *>(path));
    if (!std::filesystem::is_regular_file(file_path, error))
        return nullptr;
    const auto length = std::filesystem::file_size(file_path, error);
    if (error || length == 0 || length > disc_import_limit)
        return nullptr;
    FILE *file = ::_wfopen(file_path.c_str(), L"rb");
    if (!file)
        return nullptr;
    *size = static_cast<std::size_t>(length);
    return file;
}
#else
static FILE *open_image(const char *path, std::size_t *size) {
    const int descriptor = ::open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0)
        return nullptr;
    struct stat info{};
    if (::fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
        static_cast<std::uint64_t>(info.st_size) > disc_import_limit) {
        ::close(descriptor);
        return nullptr;
    }
    FILE *file = ::fdopen(descriptor, "rb");
    if (!file) {
        ::close(descriptor);
        return nullptr;
    }
    *size = static_cast<std::size_t>(info.st_size);
    return file;
}

#endif

static bool ends_with_cue(const char *path) {
    const auto size = std::strlen(path);
    return size >= 4 && path[size - 4] == '.' &&
        std::tolower(static_cast<unsigned char>(path[size - 3])) == 'c' &&
        std::tolower(static_cast<unsigned char>(path[size - 2])) == 'u' &&
        std::tolower(static_cast<unsigned char>(path[size - 1])) == 'e';
}

static FILE *open_disc(const char *source, std::size_t *size) {
    FILE *file = open_image(source, size);
    if (!file || !ends_with_cue(source))
        return file;
    if (*size > cue_file_capacity) {
        std::fclose(file);
        return nullptr;
    }
    ByteBuffer cue{};
    bool ok = buffer_resize(&cue, *size + 1) && std::fread(cue.data, 1, *size, file) == *size;
    if (std::fclose(file) != 0)
        ok = false;
    char name[cue_line_capacity];
    if (ok) {
        cue.data[*size] = 0;
        ok = !std::memchr(cue.data, 0, *size) &&
            disc_cue_image(reinterpret_cast<const char *>(cue.data), name, sizeof name);
    }
    buffer_release(&cue);
    if (!ok) {
        std::fprintf(stderr, "CUE must describe one MODE2/2352 track with INDEX 01 at 00:00:00.\n");
        return nullptr;
    }
    const char *slash = std::strrchr(source, '/');
#ifdef _WIN32
    if (const char *backslash = std::strrchr(source, '\\'); backslash && (!slash || backslash > slash))
        slash = backslash;
#endif
    const auto prefix = slash ? static_cast<std::size_t>(slash + 1 - source) : 0;
    char path[disc_path_capacity];
    if (prefix + std::strlen(name) >= sizeof path)
        return nullptr;
    std::memcpy(path, source, prefix);
    std::strcpy(path + prefix, name);
    return open_image(path, size);
}

#ifdef _WIN32
static bool write_asset(const std::filesystem::path &root, const Asset &asset) {
    std::error_code error;
    const auto path = root / std::filesystem::path(reinterpret_cast<const char8_t *>(asset.path));
    std::filesystem::create_directories(path.parent_path(), error);
    if (error || std::filesystem::exists(path, error))
        return false;
    FILE *file = ::_wfopen(path.c_str(), L"wb");
    if (!file)
        return false;
    const bool written = std::fwrite(asset.bytes.data, 1, asset.bytes.size, file) == asset.bytes.size;
    return std::fclose(file) == 0 && written;
}
#else
static bool write_asset(int root, const Asset &asset) {
    char path[sizeof asset.path];
    std::memcpy(path, asset.path, sizeof path);
    int directory = ::dup(root);
    if (directory < 0)
        return false;
    char *name = path;
    while (char *slash = std::strchr(name, '/')) {
        *slash = 0;
        if (::mkdirat(directory, name, S_IRWXU) != 0 && errno != EEXIST) {
            ::close(directory);
            return false;
        }
        const int next = ::openat(directory, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        ::close(directory);
        if (next < 0)
            return false;
        directory = next;
        name = slash + 1;
    }
    const int file = ::openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
    ::close(directory);
    if (file < 0)
        return false;
    std::size_t written = 0;
    while (written < asset.bytes.size) {
        const auto count = ::write(file, asset.bytes.data + written, asset.bytes.size - written);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            break;
        written += static_cast<std::size_t>(count);
    }
    const bool closed = ::close(file) == 0;
    return closed && written == asset.bytes.size;
}

#endif

bool disc_import_image(const char *source, AssetTable *assets, DiscMatch *match) {
    std::size_t size;
    FILE *file = open_disc(source, &size);
    if (!file) {
        std::fprintf(stderr, "Cannot open disc image: %s\n", source);
        return false;
    }
    auto *importer = new (std::nothrow) DiscImporter{};
    if (!importer) {
        std::fclose(file);
        return false;
    }
    disc_import_start(importer, size);
    ByteBuffer read{};
    while (disc_import_waiting(importer)) {
        const auto request = importer->request;
        if (!buffer_resize(&read, request.length) ||
#ifdef _WIN32
            ::_fseeki64(file, static_cast<long long>(request.offset), SEEK_SET) != 0 ||
#else
            ::fseeko(file, static_cast<off_t>(request.offset), SEEK_SET) != 0 ||
#endif
            std::fread(read.data, 1, read.size, file) != read.size) {
            disc_import_fail(importer, "Cannot read the selected disc image.");
            break;
        }
        disc_import_supply(importer, read.data, read.size);
    }
    buffer_release(&read);
    bool ok = importer->state == ImportState::complete;
    if (std::fclose(file) != 0)
        ok = false;
    if (!ok)
        std::fprintf(stderr, "%s\n", importer->message);
    if (ok) {
        *assets = importer->assets;
        importer->assets = {};
        *match = assets_classify(assets);
    }
    disc_import_release(importer);
    delete importer;
    return ok;
}

#ifdef _WIN32
static std::filesystem::path native_path(const char *path) {
    return std::filesystem::path(reinterpret_cast<const char8_t *>(path));
}
#endif

bool resource_tree_write(const AssetTable &assets, const char *destination) {
#ifdef _WIN32
    // Written beside the destination first and renamed when complete, so an
    // interrupted write never leaves a partial tree that looks usable.
    std::error_code error;
    const auto root = native_path(destination);
    auto partial = root;
    partial += ".partial";
    std::filesystem::remove_all(partial, error);
    if (std::filesystem::exists(root, error)) {
        std::fprintf(stderr, "Cannot write %s: it already exists.\n", destination);
        return false;
    }
    std::filesystem::create_directories(partial, error);
    bool ok = !error;
    for (std::size_t i = 0; ok && i < assets.count; ++i)
        ok = write_asset(partial, assets.entries[i]);
    if (ok) {
        std::filesystem::rename(partial, root, error);
        ok = !error;
    }
    if (!ok) {
        std::fprintf(stderr, "Cannot write the game files to %s.\n", destination);
        std::filesystem::remove_all(partial, error);
        return false;
    }
#else
    if (::mkdir(destination, S_IRWXU) != 0) {
        std::fprintf(stderr, "Cannot create %s: %s. Extraction requires a new directory; use --data to reuse one.\n",
                     destination, std::strerror(errno));
        return false;
    }
    const int root = ::open(destination, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    bool ok = root >= 0;
    for (std::size_t i = 0; ok && i < assets.count; ++i)
        ok = write_asset(root, assets.entries[i]);
    if (root >= 0 && ::close(root) != 0)
        ok = false;
    if (!ok) {
        std::fprintf(stderr, "Extraction failed; partial files remain in %s. No existing directory was replaced.\n", destination);
        return false;
    }
#endif
    std::printf("Wrote %zu game files to %s\n", assets.count, destination);
    return true;
}

bool resource_tree_read(const char *directory, AssetTable *assets) {
    // Exactly the retail file list, so nothing else in the folder is read.
    for (const auto &entry : slps00017_layout) {
        std::string path = directory;
        path += '/';
        path += entry.path;
        // Windows drops a trailing dot from file names ("E0." is stored as "E0").
        if (!path.empty() && path.back() == '.')
            path.pop_back();
        std::size_t size;
        FILE *file = open_image(path.c_str(), &size);
        if (!file && entry.size == 0) {
            ByteBuffer empty{};
            if (!assets_append(assets, entry.path, &empty))
                return false;
            continue;
        }
        if (!file)
            return false;
        ByteBuffer bytes{};
        bool ok = size == entry.size && buffer_resize(&bytes, size) && std::fread(bytes.data, 1, size, file) == size;
        std::fclose(file);
        ok = ok && assets_append(assets, entry.path, &bytes);
        buffer_release(&bytes);
        if (!ok)
            return false;
    }
    return true;
}

bool disc_extract(const char *source, const char *destination) {
    AssetTable assets{};
    DiscMatch match;
    const bool ok = disc_import_image(source, &assets, &match) && resource_tree_write(assets, destination);
    assets_release(&assets);
    return ok;
}
}
