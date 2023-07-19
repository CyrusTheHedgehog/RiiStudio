#include "MkwDebug.hpp"

#include <dolphin-memory-engine-rs/include/dme.h>
#include <imcxx/Widgets.hpp>
#include <librii/live_mkw/live_mkw.hpp>

#include <format>

#include <frontend/EditorFactory.hpp>
#include <librii/szs/SZS.hpp>

namespace live {
using namespace librii::live_mkw;
}

namespace riistudio::frontend {

DolphinAccessor dolphin;
live::Io io = [](u32 addr, std::span<u8> dst) {
  if (dolphin.getStatus() != DolphinAccessor::Status::Hooked) {
    return false;
  }
  return dolphin.readFromRAM(addr, dst, false);
};
std::string gameName() {
  std::string id = "????";
  if (dolphin.getStatus() == DolphinAccessor::Status::Hooked) {
    dolphin.readFromRAM(0x8000'0000, {(u8*)id.data(), id.size()}, false);
  }
  return id;
}
Result<std::vector<live::Info>> archives() {
  auto scn = TRY(live::GetGameScene(io));
  return live::GameScene_ReadArchives(io, scn);
}
std::string FormatSize(u32 node_size) {
  std::vector<std::string> suffixes = {"B", "KB", "MB", "GB", "TB", "PB"};

  auto size_tier = static_cast<int>(std::log10(node_size)) / 3;
  auto suffix =
      size_tier < suffixes.size() ? suffixes[size_tier] : suffixes.back();

  if (size_tier == 0) {
    return std::format("{} {}", node_size, suffix);
  } else {
    return std::format(
        "{} {}",
        static_cast<std::size_t>(node_size / std::pow(1000, size_tier)),
        suffix);
  }
}

std::vector<std::unique_ptr<riistudio::frontend::IWindow>> windows;

void MkwDebug::draw() {
#ifdef __APPLE__
  if (ImGui::Begin("Warning")) {
    ImGui::Text(
        "The integrated Dolphin debugger is unsupported on MacOS for now");
  }
  ImGui::End();
  return;
#endif
  if (ImGui::Begin("Hi!")) {
    auto status = dolphin.getStatus();
    if (ImGui::Button("Hook")) {
      dolphin.hook();
    }
    ImGui::SameLine();
    if (ImGui::Button("Unhook")) {
      dolphin.unhook();
    }
    ImGui::SameLine();
    auto name = gameName();
    ImGui::Text("STATUS: %s (%s)\n", magic_enum::enum_name(status).data(),
                name.c_str());
    ImGui::Separator();
  }
  ImGui::End();

  if (ImGui::Begin("Archives")) {
    auto arcs = archives();
    if (!arcs) {
      util::PushErrorSyle();
      ImGui::Text("%s\n", arcs.error().c_str());
      util::PopErrorStyle();
    } else {
      auto inner = *arcs;
      struct Entry {
        const char* category = "?";
        const char* heap = "?";
        u32 file_size = 0;
        live::DvdArchive low;
      };
      std::vector<Entry> entries;
      for (auto& x : inner) {
        auto entry = *x.arc.archives.get(io, 0);
        entries.push_back(Entry{
            .category = magic_enum::enum_name(x.type).data(),
            .heap = static_cast<u32>(entry.mArchiveStart.data) >= 0x9000'0000
                        ? "MEM2"
                        : "MEM1",
            .file_size = static_cast<u32>(entry.mArchiveSize),
            .low = entry,
        });
      }
      u32 flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
      if (ImGui::BeginTable("Archives", 4, flags)) {
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Filesize");
        ImGui::TableSetupColumn("Heap");
        ImGui::TableSetupColumn("Launch");
        ImGui::TableHeadersRow();
        int i = 0;
        for (auto& e : entries) {
          util::IDScope g(i++);
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(e.category);
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(e.heap);
          ImGui::TableNextColumn();
          auto s = FormatSize(e.file_size);
          ImGui::Text("%s", s.c_str());
          ImGui::TableNextColumn();
          if (ImGui::Button("GO")) {
            u32 begin = e.low.mArchiveStart.data;
            u32 size = e.low.mArchiveSize;
            std::vector<u8> buf(size);
            dolphin.readFromRAM(begin, buf, false);
            auto b2 = librii::szs::encodeCTGP(buf);
            auto path = std::format("{}.szs", e.category);
            auto ed = frontend::MakeEditor(*b2, path);
            if (ed) {
              windows.push_back(std::move(ed));
            }
          }
        }
        ImGui::EndTable();
      }
    }
  }
  ImGui::End();
  for (auto& win : windows) {
    if (!win) {
      continue;
    }
    win->draw();
    if (!win->isOpen()) {
      win = nullptr;
    }
  }
}

} // namespace riistudio::frontend