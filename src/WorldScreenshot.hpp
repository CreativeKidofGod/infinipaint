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
#include <functional>
#include <optional>
#include <vector>
#include <filesystem>
#include "CoordSpaceHelper.hpp"
#include <Helpers/SCollision.hpp>

class SkCanvas;
struct DrawData;
class World;

struct WorldScreenshotInfo {
    enum class ScreenshotType : size_t {
        JPG,
        PNG,
        WEBP,
        SVG
    };
    std::filesystem::path filePath;
    ScreenshotType type;
    Vector2i imageSizePixels;
    CoordSpaceHelper cameraCoords;
    SCollision::AABB<float> imageBounds;
    bool transparentBackground;
    bool displayGrid;
    bool share = false;
};

NLOHMANN_JSON_SERIALIZE_ENUM(WorldScreenshotInfo::ScreenshotType, {
    {WorldScreenshotInfo::ScreenshotType::JPG, "jpg"},
    {WorldScreenshotInfo::ScreenshotType::PNG, "png"},
    {WorldScreenshotInfo::ScreenshotType::WEBP, "webp"},
    {WorldScreenshotInfo::ScreenshotType::SVG, "svg"}
})

void world_take_screenshot(const std::shared_ptr<World>& w, const WorldScreenshotInfo& info);
// Custom fork: renders a screen-space region (in cameraCoords) to premultiplied RGBA pixels, drawing only what drawFunc draws
// on a transparent background. Returns nothing on failure.
std::optional<std::vector<uint8_t>> world_render_region_rgba(const std::shared_ptr<World>& w, const CoordSpaceHelper& cameraCoords, const SCollision::AABB<float>& imageBounds, const Vector2i& imageSizePixels, const std::function<void(SkCanvas*, const DrawData&)>& drawFunc);
std::string world_screenshot_info_get_extension_from_type(WorldScreenshotInfo::ScreenshotType t);
std::string world_screenshot_info_get_mime_from_type(WorldScreenshotInfo::ScreenshotType t);
