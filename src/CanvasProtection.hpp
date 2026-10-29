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
#include <string>

// Custom fork: stops File Explorer (or anything else) from deleting, renaming or moving canvases in the canvas folder,
// while this app can still save them. Windows only; does nothing elsewhere.
//
// How: each canvas file gets a "deny Delete" rule for Everyone, and the canvas folder gets a "deny Delete Child"
// rule (Windows allows deleting a file if either the file or its folder allows it, so both are needed). Other files
// in the folder keep their own Delete permission, so they're unaffected. Before this app replaces a canvas file it
// lifts the file's rule, then puts it back.
namespace CanvasProtection {
    void protect_folder(const std::filesystem::path& folder);
    void unprotect_folder(const std::filesystem::path& folder);
    void protect_file(const std::filesystem::path& file);
    void unprotect_file(const std::filesystem::path& file); // Safe to call on any file, protected or not

    // Every canvas directly in the folder, plus the folder itself
    void protect_all(const std::filesystem::path& folder);
    void unprotect_all(const std::filesystem::path& folder);

    bool is_in_folder(const std::filesystem::path& file, const std::filesystem::path& folder);

    // Lifts protection and sends the file to the Recycle Bin (so it can be restored)
    bool move_to_recycle_bin(const std::filesystem::path& file, std::string& errorOut);
}
