project(rsl)

include_directories(${PROJECT_SOURCE_DIR}/../)
include_directories(${PROJECT_SOURCE_DIR}/../vendor)
include_directories(${PROJECT_SOURCE_DIR}/../plate/include)
include_directories(${PROJECT_SOURCE_DIR}/../plate/vendor)


add_library(rsl STATIC
  "DebugBreak.hpp"
  "Defer.hpp"
  "Discord.cpp"
  "Download.cpp"
  "FsDialog.cpp"
  "Launch.cpp"
  "Log.cpp"
  "Ranges.hpp"
  "SafeReader.cpp"
  "Stb.cpp"
  "WriteFile.cpp"
  "Zip.cpp"
)
 
if (WIN32)
  target_link_directories(rsl PUBLIC ${PROJECT_SOURCE_DIR}/../vendor/dll)
endif()

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml CRATE_TYPES staticlib FLAGS --crate-type=staticlib)

target_link_libraries(rsl PUBLIC core riistudio_rs vendor)

if (NOT EMSCRIPTEN)
  target_link_libraries(rsl PUBLIC llvm_sighandler c-discord-rich-presence)
endif()
