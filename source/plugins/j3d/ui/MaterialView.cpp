#include <core/kpi/PropertyView.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <plugins/gc/Export/Material.hpp>
#include <vendor/fa5/IconsFontAwesome5.h>

#include <plugins/j3d/Joint.hpp>
#include <plugins/j3d/Material.hpp>
#include <plugins/j3d/Scene.hpp>
#include <plugins/j3d/Shape.hpp>

#include <core/util/gui.hpp>
#include <imcxx/Widgets.hpp>

namespace riistudio::j3d::ui {

struct J3DDataSurface final {
  static inline const char* name = "J3D Data";
  static inline const char* icon = (const char*)ICON_FA_BOXES;
};

void drawProperty(kpi::PropertyDelegate<Material>& delegate, J3DDataSurface) {
  int flag = delegate.getActive().flag;
  ImGui::InputInt("Flag", &flag, 1, 1);
  KPI_PROPERTY_EX(delegate, flag, static_cast<u8>(flag));

  librii::gx::ColorF32 clr_f32;

  if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
    auto& fog = delegate.getActive().fogInfo;

    int orthoOrPersp =
        static_cast<int>(fog.type) >=
                static_cast<int>(librii::j3d::Fog::Type::OrthographicLinear)
            ? 0
            : 1;
    int fogType = static_cast<int>(fog.type);
    if (fogType >= static_cast<int>(librii::j3d::Fog::Type::OrthographicLinear))
      fogType -= 5;
    ImGui::Combo("Projection", &orthoOrPersp, "Orthographic\0Perspective\0");
    ImGui::Combo("Function", &fogType,
                 "None\0Linear\0Exponential\0Quadratic\0Inverse "
                 "Exponential\0Quadratic\0");
    int new_type = fogType;
    if (new_type != 0)
      new_type += (1 - orthoOrPersp) * 5;
    KPI_PROPERTY_EX(delegate, fogInfo.type,
                    static_cast<librii::j3d::Fog::Type>(new_type));
    bool enabled = fog.enabled;
    ImGui::Checkbox("Fog Enabled", &enabled);
    KPI_PROPERTY_EX(delegate, fogInfo.enabled, enabled);

    {
      util::ConditionalActive g(enabled /* && new_type != 0*/);
      ImGui::PushItemWidth(200);
      {
        int center = fog.center;
        ImGui::InputInt("Center", &center);
        KPI_PROPERTY_EX(delegate, fogInfo.center, static_cast<u16>(center));

        float startZ = fog.startZ;
        ImGui::InputFloat("Start Z", &startZ);
        KPI_PROPERTY_EX(delegate, fogInfo.startZ, startZ);
        ImGui::SameLine();
        float endZ = fog.endZ;
        ImGui::InputFloat("End Z", &endZ);
        KPI_PROPERTY_EX(delegate, fogInfo.endZ, endZ);

        float nearZ = fog.nearZ;
        ImGui::InputFloat("Near Z", &nearZ);
        KPI_PROPERTY_EX(delegate, fogInfo.nearZ, nearZ);
        ImGui::SameLine();
        float farZ = fog.farZ;
        ImGui::InputFloat("Far Z", &farZ);
        KPI_PROPERTY_EX(delegate, fogInfo.farZ, farZ);
      }
      ImGui::PopItemWidth();
      clr_f32 = fog.color;
      ImGui::ColorEdit4("Fog Color", clr_f32);
      KPI_PROPERTY_EX(delegate, fogInfo.color, (librii::gx::Color)clr_f32);

      // RangeAdjTable? Maybe a graph?
    }
  }

  if (ImGui::CollapsingHeader("Light Colors (Usually ignored by games)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {

    int i = 0;
    for (auto& clr : delegate.getActive().lightColors) {
      clr_f32 = clr;
      ImGui::ColorEdit4(
          (std::string("Light Color ") + std::to_string(i)).c_str(), clr_f32);

      KPI_PROPERTY(delegate, clr, (librii::gx::Color)clr_f32, lightColors[i]);
      ++i;
    }
  }
  if (ImGui::CollapsingHeader("NBT Scale", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool enabled = delegate.getActive().nbtScale.enable;
    auto scale = delegate.getActive().nbtScale.scale;

    ImGui::Checkbox("NBT Enabled", &enabled);
    KPI_PROPERTY_EX(delegate, nbtScale.enable, enabled);

    {
      util::ConditionalActive g(enabled);

      ImGui::InputFloat3("Scale", &scale.x);
      KPI_PROPERTY_EX(delegate, nbtScale.scale, scale);
    }
  }
}
struct BoneJ3DSurface final {
  static inline const char* name = "J3D Data";
  static inline const char* icon = (const char*)ICON_FA_BOXES;
};

void drawProperty(kpi::PropertyDelegate<Joint>& delegate, BoneJ3DSurface) {
  auto& bone = delegate.getActive();

  int flag = bone.flag;
  ImGui::InputInt("Flag", &flag);
  KPI_PROPERTY_EX(delegate, flag, static_cast<u16>(flag));

  auto bbMtx = imcxx::Combo("Billboard Matrix", bone.bbMtxType,
                            "Standard\0"
                            "XY\0"
                            "Y\0");
  KPI_PROPERTY_EX(delegate, bbMtxType, bbMtx);

  auto boundingBox = bone.boundingBox;
  auto boundingSphereRadius = bone.boundingSphereRadius;
  Toolkit::BoundingVolume(&boundingBox, &boundingSphereRadius);
  KPI_PROPERTY_EX(delegate, boundingBox, boundingBox);
  KPI_PROPERTY_EX(delegate, boundingSphereRadius, boundingSphereRadius);

  const riistudio::lib3d::Model* pMdl =
      dynamic_cast<const riistudio::lib3d::Model*>(
          dynamic_cast<const kpi::IObject*>(&bone)->childOf);
  const auto mtx = delegate.getActive().calcSrtMtx(pMdl);

  ImGui::Text("Computed Matrix:");
  Toolkit::Matrix44(mtx);
}

struct ShapeJ3DSurface final {
  static inline const char* name = "J3D Shape";
  static inline const char* icon = (const char*)ICON_FA_BOXES;
};
void drawProperty(kpi::PropertyDelegate<Shape>& dl, ShapeJ3DSurface) {
  auto& shape = dl.getActive();

  auto mode = imcxx::Combo("Mode", shape.mode,
                           "Standard\0"
                           "Billboard XY\0"
                           "Billboard Y\0"
                           "Skinned\0");
  KPI_PROPERTY_EX(dl, mode, mode);

  auto bbox = shape.bbox;
  auto bsphere = shape.bsphere;
  Toolkit::BoundingVolume(&bbox, &bsphere);
  KPI_PROPERTY_EX(dl, bbox, bbox);
  KPI_PROPERTY_EX(dl, bsphere, bsphere);

  bool vis = shape.visible;
  ImGui::Checkbox("Visible", &vis);
  KPI_PROPERTY_EX(dl, visible, vis);
  int i = 0;
  for (auto& mp : shape.mMatrixPrimitives) {
    ImGui::Text("Matrix Primitive: %i", i);

    const auto matrices =
        shape.getPosMtx(*dynamic_cast<libcube::Model*>(shape.childOf), i);
    int j = 0;
    for (auto& elem : mp.mDrawMatrixIndices) {
      ImGui::Text("DRW %i: %i", j, elem);
      Toolkit::Matrix44(matrices[j]);
      ++j;
    }
    ++i;
  }
}
struct ModelJ3DSurface {
  static inline const char* name = "J3D Model";
  static inline const char* icon = (const char*)ICON_FA_ADDRESS_BOOK;
};
void drawProperty(kpi::PropertyDelegate<j3d::Model>& dl, ModelJ3DSurface) {
  auto& mdl = dl.getActive();

  int sclRule = static_cast<int>(mdl.info.mScalingRule);
  ImGui::Combo("Scaling Rule", &sclRule, "Basic\0XSI\0Maya\0");
  KPI_PROPERTY_EX(dl, info.mScalingRule,
                  static_cast<Model::Information::ScalingRule>(sclRule));

  if (ImGui::CollapsingHeader("Draw Matrices / Envelopes",
                              ImGuiTreeNodeFlags_DefaultOpen) &&
      ImGui::BeginChild("Entries")) {
    int i = 0;
    for (const auto& drw : mdl.mDrawMatrices) {
      if (ImGui::CollapsingHeader((std::string("Matrix ") +
                                   std::to_string(i++) + "(Total" +
                                   std::to_string(drw.mWeights.size()) + ")")
                                      .c_str())) {
        util::IDScope g_i(i);
        int j = 0;
        for (const auto& w : drw.mWeights) {
          if (ImGui::CollapsingHeader(
                  (std::string("Weight ") + std::to_string(j++)).c_str()),
              ImGuiTreeNodeFlags_DefaultOpen) {
            util::IDScope g_j(j);
            int boneId = w.boneId;
            float weight = w.weight;
            ImGui::InputInt("Bone", &boneId);
            ImGui::SameLine();
            ImGui::InputFloat("Influence", &weight);
          }
        }
      }
    }

    ImGui::EndChild();
  }
}

kpi::DecentralizedInstaller Installer([](kpi::ApplicationPlugins&) {
  auto& inst = kpi::PropertyViewManager::getInstance();
  inst.addPropertyView<Material, J3DDataSurface>();
  inst.addPropertyView<Joint, BoneJ3DSurface>();
  inst.addPropertyView<Shape, ShapeJ3DSurface>();
  inst.addPropertyView<Model, ModelJ3DSurface>();
});

} // namespace riistudio::j3d::ui
