#include <core/applet.hpp> // core::Markdown
#include <core/util/gui.hpp>

namespace riistudio {

void DrawChangeLog(bool* show = nullptr) {
  if (show != nullptr && !*show)
    return;

  if (ImGui::Begin("Changelog", show, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("RiiStudio: Alpha 2.0 Pre-Release");
    const std::string& markdownText = u8R"(
UI Overhaul:
  * New TEV, Lighting, Pixel Shader, Swap Table, Draw Call, Vertex Data, and Texture property editors.

BMD Saving:
  * Note: BDL files currently will be saved as BMD. This incurs no data loss.

Misc:
  * Added vertical tabs and header visibility toggles to the property editor.
  * Fixed several shadergen bugs.
  * Added support for BRRES files with basic rigging.
  * Multiple instances of property editors on the same tab can work in parallel.
  * Fixed bug where rotation was converted to radians twice for BMD SRT matrices.
  * Fixed culling mode being read incorrectly in BMD files.
  * Automatically set camera clipping planes.
  * Fixed handling compression LUTs in BMD files.
  * Fixed rendering BMD files with varying vertex descriptors.
)";
    core::Markdown(markdownText);

    ImGui::End();
  }
}

} // namespace riistudio
