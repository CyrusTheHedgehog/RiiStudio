#pragma once

#include <frontend/widgets/IconDatabase.hpp>
#include <rsl/DenseMap.hpp>

namespace riistudio {

class IconManager {
public:
  IconManager();
  IconManager(const IconManager&) = delete;
  IconManager(IconManager&&) = delete;
  ~IconManager() = default;

  void propagateIcons(kpi::ICollection& folder);
  void propagateIcons(kpi::INode& node);
  // Will upload if missing
  void drawImageIcon(const lib3d::Texture* tex, u32 dim);

  static IconManager* get() { return sInstance; }

private:
  static IconManager* sInstance;
  IconDatabase mIconManager;
  rsl::dense_map<lib3d::GenerationIDTracked::GenerationID, IconDatabase::Key>
      mImageIcons;
  std::unique_ptr<lib3d::Texture> mNullIcon;
};

} // namespace riistudio
