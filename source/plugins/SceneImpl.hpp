#include <LibBadUIFramework/Node2.hpp>
#include <core/3d/i3dmodel.hpp>
#include <glm/mat4x4.hpp>
#include <librii/g3d/gfx/G3dGfx.hpp>
#include <librii/gfx/SceneState.hpp>

namespace librii::crate {
struct CrateAnimationJ3D;
}

namespace librii::j3d {
struct MaterialData;
}

namespace riistudio::j3d {

class Collection;

Result<void>
ApplyCratePresetToMaterialJ3D(riistudio::j3d::Collection& scene,
                              librii::j3d::MaterialData& target_mat,
                              const librii::crate::CrateAnimationJ3D& src_mat);

} // namespace riistudio::j3d
