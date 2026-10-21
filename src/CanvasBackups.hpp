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

#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class World;

// Custom fork: keeps the last few backups of every canvas in the settings folder
// (<config>/backups), away from the canvas folder so they survive the canvas file being deleted.
//
// Layout:
//   backups/media/<hash>.bin          one file per picture/file, shared by every backup that uses it
//   backups/canvases/<key>/source.txt the canvas's original path (UTF-8)
//   backups/canvases/<key>/<time>.ipbk a backup: the canvas without its pictures, plus which media it uses
//
// A backup can be turned back into a normal .infpnt file that both this app and official InfiniPaint open.
namespace CanvasBackups {
    constexpr int BACKUPS_TO_KEEP = 3;
    constexpr int MINUTES_BETWEEN_BACKUPS = 30;

    // Called after every successful save. Makes a backup if the newest one is older than MINUTES_BETWEEN_BACKUPS.
    void after_save(World& w);

    struct Backup {
        std::filesystem::path file;
        std::string timeText; // "2026-10-21 14:05"
    };

    struct CanvasBackupList {
        std::string canvasName;
        std::filesystem::path originalPath;
        bool originalStillExists = false;
        std::vector<Backup> backups; // Newest first
    };

    // Every canvas that has backups, most recently backed up first
    std::vector<CanvasBackupList> list_all(const std::filesystem::path& configPath);

    // Rebuilds the backup as "<name> (Backup <time>).infpnt" in folder. Never overwrites anything.
    std::optional<std::filesystem::path> restore_to_folder(const CanvasBackupList& canvas, const Backup& backup, const std::filesystem::path& folder, std::string& errorOut);
}
