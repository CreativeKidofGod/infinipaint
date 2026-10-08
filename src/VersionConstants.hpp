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
#include <Helpers/VersionNumber.hpp>
#include <unordered_map>

namespace VersionConstants {
    // Map OLDEST version that is compatible with the filetype with a specific header
    // This means that, to check for file version, you can do these two checks:
    // Check if older than a certain version: if(fileVersion < NEWEST VERSION YOU SHOULDNT RUN THE IF STATEMENT ON)
    // Check if newer/equal to a certain version: if(fileVersion >= OLDEST VERSION YOU SHOULD RUN THE IF STATEMENT ON)
    VersionNumber header_to_version_number(const std::string& header); 

    constexpr int SAVEFILE_HEADER_LEN = 12; // DO NOT CHANGE THIS HEADER LENGTH
    // Custom fork: canvases are saved with their own header, so official InfiniPaint refuses them cleanly
    // instead of misreading the extra layer data. Old official headers still load.
    const std::string CURRENT_SAVEFILE_HEADER = "INFPNTCUS001"; // Change whenever the save file is incompatible with the previous version
    constexpr VersionNumber CUSTOM_LAYER_KIND_VERSION(0, 6, 100); // Layers store a LayerKind
    const std::string CURRENT_VERSION_STRING = "0.6.1";
    constexpr VersionNumber CURRENT_VERSION_NUMBER(0, 6, 1);

    // Custom fork build tag (CreativeKidofGod/infinipaint). Kept separate from
    // CURRENT_VERSION_STRING, which must stay a plain "x.y.z" because config.json
    // and the update checker parse it as a number.
    const std::string CUSTOM_BUILD_TAG = "custom.6";
    const std::string DISPLAY_VERSION_STRING = CURRENT_VERSION_STRING + "-" + CUSTOM_BUILD_TAG;
}
