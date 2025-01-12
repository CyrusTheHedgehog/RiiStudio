#pragma once

#include <core/util/timestamp.hpp>
#include <librii/image/ImagePlatform.hpp>
#include <librii/szs/SZS.hpp>

namespace riistudio::frontend {

void DrawVersionInfo() {
  ImGui::Text("%s", RII_TIME_STAMP);
  std::string gctex_ver =
      "gctex " + std::string(librii::image::gctex_version());
  ImGui::Text("%s", gctex_ver.c_str());
  std::string szs_ver = "szs " + std::string(librii::szs::szs_version());
  ImGui::Text("%s", szs_ver.c_str());
}

} // namespace riistudio::frontend
