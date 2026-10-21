/*
 * InfiniPaint
 * Copyright (C) 2025-2026 Yousef Khadadeh
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "CanvasBackups.hpp"
#include "World.hpp"
#include "MainProgram.hpp"
#include "VersionConstants.hpp"
#include <Helpers/Logger.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/string.hpp>
#include <zstd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace CanvasBackups {

namespace {

const std::string MANIFEST_MAGIC = "IPBK0001";
constexpr size_t MEDIA_HASH_LEN = 32;

std::string to_utf8(const std::filesystem::path& p) {
    std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::filesystem::path from_utf8(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

uint64_t fnv1a(std::string_view s, uint64_t basis) {
    uint64_t h = basis;
    for(unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t mix(uint64_t x) {
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

std::string hex64(uint64_t v) {
    static const char* digits = "0123456789abcdef";
    std::string out(16, '0');
    for(int i = 15; i >= 0; i--) {
        out[i] = digits[v & 0xF];
        v >>= 4;
    }
    return out;
}

// 128 bits from two different 64-bit hashes, so two different pictures practically never share a name
std::string media_hash(std::string_view data) {
    uint64_t h1 = fnv1a(data, 14695981039346656037ULL);
    uint64_t h2 = mix(fnv1a(data, 0x84222325cbf29ce4ULL) ^ static_cast<uint64_t>(data.size()));
    return hex64(h1) + hex64(h2);
}

// cereal's portable archive writes one endianness byte first. Pieces are glued back
// together under a single archive's byte, so each piece drops its own.
template <typename T> std::string archive_bytes(const T& t) {
    std::stringstream ss;
    {
        cereal::PortableBinaryOutputArchive a(ss);
        a(t);
    }
    std::string s = ss.str();
    return s.empty() ? s : s.substr(1);
}

std::string size_tag_bytes(uint64_t count) {
    std::stringstream ss;
    {
        cereal::PortableBinaryOutputArchive a(ss);
        a(cereal::make_size_tag(static_cast<cereal::size_type>(count)));
    }
    return ss.str().substr(1);
}

void write_u64(std::string& out, uint64_t v) {
    for(int i = 0; i < 8; i++)
        out.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
}

bool read_u64(std::string_view& in, uint64_t& v) {
    if(in.size() < 8)
        return false;
    v = 0;
    for(int i = 0; i < 8; i++)
        v |= static_cast<uint64_t>(static_cast<unsigned char>(in[i])) << (8 * i);
    in.remove_prefix(8);
    return true;
}

std::string read_whole_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if(!f)
        throw std::runtime_error("Couldn't read " + to_utf8(p));
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Write to a temporary file, then swap it in
void write_whole_file(const std::filesystem::path& p, std::string_view data) {
    std::filesystem::path temp = p;
    temp += ".saving";
    {
        std::ofstream f(temp, std::ios::binary | std::ios::trunc);
        if(!f)
            throw std::runtime_error("Couldn't write " + to_utf8(temp));
        f.write(data.data(), data.size());
        if(!f)
            throw std::runtime_error("Couldn't write " + to_utf8(temp));
    }
    std::filesystem::rename(temp, p);
}

std::filesystem::path backups_root(const std::filesystem::path& configPath) {
    return configPath / "backups";
}

std::string canvas_key(const std::filesystem::path& canvasPath) {
    std::string stem = to_utf8(canvasPath.stem());
    std::string safe;
    for(char c : stem) {
        unsigned char u = static_cast<unsigned char>(c);
        if(std::isalnum(u) || c == ' ' || c == '-' || c == '_')
            safe.push_back(c);
        else if(u < 0x80)
            safe.push_back('_');
        // Non-ASCII bytes are left out; the hash below keeps keys unique
        if(safe.size() >= 40)
            break;
    }
    std::string fullPath = to_utf8(std::filesystem::absolute(canvasPath).lexically_normal());
    for(char& c : fullPath)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); // Windows paths ignore case
    return safe + "-" + hex64(fnv1a(fullPath, 14695981039346656037ULL));
}

std::vector<std::filesystem::path> list_backup_files(const std::filesystem::path& canvasDir) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for(auto& entry : std::filesystem::directory_iterator(canvasDir, ec)) {
        if(entry.is_regular_file(ec) && entry.path().extension() == ".ipbk")
            files.emplace_back(entry.path());
    }
    std::sort(files.begin(), files.end()); // Names are timestamps, so this is oldest first
    return files;
}

std::string timestamp_now() {
    std::time_t t = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &local);
    return buf;
}

// "20261021-140512" -> "2026-10-21 14:05"
std::string timestamp_to_text(const std::string& stamp) {
    if(stamp.size() < 13)
        return stamp;
    return stamp.substr(0, 4) + "-" + stamp.substr(4, 2) + "-" + stamp.substr(6, 2) + " " + stamp.substr(9, 2) + ":" + stamp.substr(11, 2);
}

struct ParsedBackup {
    uint64_t prefixSize = 0;
    std::string_view compressedPrefix;
    std::vector<std::pair<std::string_view, std::string>> resources; // (key bytes, media hash)
};

ParsedBackup parse_backup(std::string_view data) {
    ParsedBackup p;
    if(data.substr(0, MANIFEST_MAGIC.size()) != MANIFEST_MAGIC)
        throw std::runtime_error("Not a backup file");
    data.remove_prefix(MANIFEST_MAGIC.size());
    uint64_t compressedSize;
    if(!read_u64(data, p.prefixSize) || !read_u64(data, compressedSize) || data.size() < compressedSize)
        throw std::runtime_error("Backup file is damaged");
    p.compressedPrefix = data.substr(0, compressedSize);
    data.remove_prefix(compressedSize);
    uint64_t resourceCount;
    if(!read_u64(data, resourceCount))
        throw std::runtime_error("Backup file is damaged");
    for(uint64_t i = 0; i < resourceCount; i++) {
        uint64_t keyLen;
        if(!read_u64(data, keyLen) || data.size() < keyLen + MEDIA_HASH_LEN)
            throw std::runtime_error("Backup file is damaged");
        std::string_view key = data.substr(0, keyLen);
        data.remove_prefix(keyLen);
        p.resources.emplace_back(key, std::string(data.substr(0, MEDIA_HASH_LEN)));
        data.remove_prefix(MEDIA_HASH_LEN);
    }
    return p;
}

// Delete pictures no backup uses anymore
void remove_unused_media(const std::filesystem::path& root) {
    std::unordered_set<std::string> used;
    std::error_code ec;
    for(auto& canvasDir : std::filesystem::directory_iterator(root / "canvases", ec)) {
        if(!canvasDir.is_directory(ec))
            continue;
        for(auto& backupFile : list_backup_files(canvasDir.path())) {
            try {
                std::string data = read_whole_file(backupFile);
                for(auto& [key, hash] : parse_backup(data).resources)
                    used.emplace(hash);
            }
            catch(...) {
                return; // If any backup can't be read, don't delete anything
            }
        }
    }
    for(auto& media : std::filesystem::directory_iterator(root / "media", ec)) {
        if(media.path().extension() == ".bin" && !used.contains(to_utf8(media.path().stem()))) {
            std::error_code removeEc;
            std::filesystem::remove(media.path(), removeEc);
        }
    }
}

}

void after_save(World& w) {
#ifndef __EMSCRIPTEN__
    if(w.filePath.empty() || w.main.conf.configPath.empty())
        return;
    try {
        std::filesystem::path root = backups_root(w.main.conf.configPath);
        std::filesystem::path canvasDir = root / "canvases" / canvas_key(w.filePath);
        std::filesystem::create_directories(canvasDir);
        std::filesystem::create_directories(root / "media");

        auto existing = list_backup_files(canvasDir);
        if(!existing.empty()) {
            std::error_code ec;
            auto newestTime = std::filesystem::last_write_time(existing.back(), ec);
            if(!ec && std::filesystem::file_time_type::clock::now() - newestTime < std::chrono::minutes(MINUTES_BETWEEN_BACKUPS))
                return;
        }

        std::string prefix;
        {
            std::stringstream ss;
            {
                cereal::PortableBinaryOutputArchive a(ss);
                w.save_file_without_resources(a);
            }
            prefix = ss.str();
        }
        std::vector<char> compressed(ZSTD_compressBound(prefix.size()));
        size_t compressedSize = ZSTD_compress(compressed.data(), compressed.size(), prefix.data(), prefix.size(), ZSTD_CLEVEL_DEFAULT);
        if(ZSTD_isError(compressedSize))
            throw std::runtime_error("Compression failed");

        std::string manifest = MANIFEST_MAGIC;
        write_u64(manifest, prefix.size());
        write_u64(manifest, compressedSize);
        manifest.append(compressed.data(), compressedSize);

        auto resources = w.rMan.get_saved_resources();
        write_u64(manifest, resources.size());
        for(auto& [id, resource] : resources) {
            std::string keyBytes = archive_bytes(id);
            std::string resourceBytes = archive_bytes(resource);
            std::string hash = media_hash(resourceBytes);
            std::filesystem::path mediaPath = root / "media" / (hash + ".bin");
            if(!std::filesystem::exists(mediaPath))
                write_whole_file(mediaPath, resourceBytes); // Stored once, shared by every backup that uses it
            write_u64(manifest, keyBytes.size());
            manifest += keyBytes;
            manifest += hash;
        }

        write_whole_file(canvasDir / "source.txt", to_utf8(std::filesystem::absolute(w.filePath)));
        std::filesystem::path backupPath = canvasDir / (timestamp_now() + ".ipbk");
        if(std::filesystem::exists(backupPath))
            return;
        write_whole_file(backupPath, manifest);

        existing = list_backup_files(canvasDir);
        for(size_t i = 0; i + BACKUPS_TO_KEEP < existing.size(); i++) {
            std::error_code ec;
            std::filesystem::remove(existing[i], ec);
        }
        remove_unused_media(root);

        Logger::get().log(Logger::LogType::INFO, "Backup made: " + to_utf8(backupPath));
    }
    catch(const std::exception& e) {
        // A failed backup must never stop saving
        Logger::get().log(Logger::LogType::INFO, std::string("Backup failed: ") + e.what());
    }
#endif
}

std::vector<CanvasBackupList> list_all(const std::filesystem::path& configPath) {
    std::vector<CanvasBackupList> toRet;
    std::error_code ec;
    for(auto& canvasDir : std::filesystem::directory_iterator(backups_root(configPath) / "canvases", ec)) {
        if(!canvasDir.is_directory(ec))
            continue;
        auto files = list_backup_files(canvasDir.path());
        if(files.empty())
            continue;
        CanvasBackupList c;
        try {
            c.originalPath = from_utf8(read_whole_file(canvasDir.path() / "source.txt"));
        }
        catch(...) {}
        c.canvasName = c.originalPath.empty() ? to_utf8(canvasDir.path().filename()) : to_utf8(c.originalPath.stem());
        std::error_code existsEc;
        c.originalStillExists = !c.originalPath.empty() && std::filesystem::exists(c.originalPath, existsEc);
        for(auto it = files.rbegin(); it != files.rend(); ++it)
            c.backups.emplace_back(Backup{*it, timestamp_to_text(to_utf8(it->stem()))});
        toRet.emplace_back(std::move(c));
    }
    std::sort(toRet.begin(), toRet.end(), [](const auto& a, const auto& b) {
        return a.backups.front().file.filename() > b.backups.front().file.filename();
    });
    return toRet;
}

std::optional<std::filesystem::path> restore_to_folder(const CanvasBackupList& canvas, const Backup& backup, const std::filesystem::path& folder, std::string& errorOut) {
    try {
        std::string data = read_whole_file(backup.file);
        ParsedBackup parsed = parse_backup(data);

        std::string worldData(parsed.prefixSize, '\0');
        size_t decompressedSize = ZSTD_decompress(worldData.data(), worldData.size(), parsed.compressedPrefix.data(), parsed.compressedPrefix.size());
        if(ZSTD_isError(decompressedSize) || decompressedSize != parsed.prefixSize)
            throw std::runtime_error("Backup file is damaged");

        std::filesystem::path root = backup.file.parent_path().parent_path().parent_path(); // backups/canvases/<key>/<file>
        worldData += size_tag_bytes(parsed.resources.size());
        for(auto& [key, hash] : parsed.resources) {
            std::filesystem::path mediaPath = root / "media" / (hash + ".bin");
            if(!std::filesystem::exists(mediaPath))
                throw std::runtime_error("A picture or file used by this backup is missing");
            worldData += key;
            worldData += read_whole_file(mediaPath);
        }

        std::vector<char> compressed(ZSTD_compressBound(worldData.size()));
        size_t compressedSize = ZSTD_compress(compressed.data(), compressed.size(), worldData.data(), worldData.size(), ZSTD_CLEVEL_DEFAULT);
        if(ZSTD_isError(compressedSize))
            throw std::runtime_error("Compression failed");
        std::string fileData = VersionConstants::CURRENT_SAVEFILE_HEADER.substr(0, VersionConstants::SAVEFILE_HEADER_LEN);
        fileData.append(compressed.data(), compressedSize);

        std::string timeForName = backup.timeText;
        std::replace(timeForName.begin(), timeForName.end(), ':', '-'); // ':' isn't allowed in Windows file names
        std::string baseName = canvas.canvasName + " (Backup " + timeForName + ")";
        std::filesystem::create_directories(folder);
        std::filesystem::path outPath = folder / from_utf8(baseName + "." + World::FILE_EXTENSION);
        for(int i = 2; std::filesystem::exists(outPath); i++)
            outPath = folder / from_utf8(baseName + " " + std::to_string(i) + "." + World::FILE_EXTENSION);
        write_whole_file(outPath, fileData);
        return outPath;
    }
    catch(const std::exception& e) {
        errorOut = e.what();
        return std::nullopt;
    }
}

}
