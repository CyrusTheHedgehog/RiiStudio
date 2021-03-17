#pragma once

#include <core/common.h>
#include <librii/gx.h>
#include <plugins/gc/Export/IndexedPolygon.hpp>
#include <vector>

#include <core/kpi/Node.hpp>

namespace riistudio::g3d {

class Model;

using MatrixPrimitive = librii::gx::MatrixPrimitive;
struct PolygonData : public librii::gx::MeshData {
  std::string mName;
  u32 mId;

  s16 mCurrentMatrix = -1; // Part of the polygon in G3D

  bool currentMatrixEmbedded = false;
  bool visible = true;

  // For IDs, set to -1 in binary to not exist. Here, empty string
  std::string mPositionBuffer;
  std::string mNormalBuffer;
  std::array<std::string, 2> mColorBuffer;
  std::array<std::string, 8> mTexCoordBuffer;

  bool operator==(const PolygonData& rhs) const = default;
};
struct Polygon : public PolygonData,
                 public libcube::IndexedPolygon,
                 public virtual kpi::IObject {
  void setId(u32 id) override { mId = id; }
  std::string getName() const { return mName; }
  void setName(const std::string& name) override { mName = name; }

  MeshData& getMeshData() override { return *this; }
  const MeshData& getMeshData() const { return *this; }
  librii::math::AABB getBounds() const override { return bounds; }
  std::vector<glm::mat4> getPosMtx(const libcube::Model& mdl,
                                   u64 mpId) const override;

  glm::vec2 getUv(const libcube::Model& mdl, u64 chan, u64 id) const override;
  glm::vec4 getClr(const libcube::Model& mdl, u64 chan, u64 id) const override;
  glm::vec3 getPos(const libcube::Model& mdl, u64 id) const override;
  glm::vec3 getNrm(const libcube::Model& mdl, u64 id) const override;
  u64 addPos(libcube::Model& mdl, const glm::vec3& v) override;
  u64 addNrm(libcube::Model& mdl, const glm::vec3& v) override;
  u64 addClr(libcube::Model& mdl, u64 chan, const glm::vec4& v) override;
  u64 addUv(libcube::Model& mdl, u64 chan, const glm::vec2& v) override;

  void init(bool skinned, librii::math::AABB* boundingBox) override {
    // TODO: Handle skinning...
    (void)skinned;
    if (boundingBox != nullptr)
      bounds = *boundingBox;
  }
  void initBufsFromVcd(lib3d::Model& mdl) override;
  bool operator==(const Polygon& rhs) const {
    return PolygonData::operator==(rhs);
  }

  librii::math::AABB bounds;
};

} // namespace riistudio::g3d
