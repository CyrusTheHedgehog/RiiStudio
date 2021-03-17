#pragma once

#include <core/3d/i3dmodel.hpp>
#include <core/3d/renderer/SceneState.hpp>
#include <core/common.h>
#include <core/kpi/Node.hpp>
#include <frontend/renderer/CameraController.hpp>
#include <glm/mat4x4.hpp>
#include <memory>

namespace riistudio::frontend {

struct SceneState;

class Renderer {
public:
  Renderer(lib3d::IDrawable* root);
  ~Renderer();
  void render(u32 width, u32 height, bool& hideCursor);
  void prepare(const kpi::INode& host) { mRoot->prepare(mSceneState, host); }

  Camera& getCamera() { return mCameraController.mCamera; }

  void drawMenuBar();
  void updateCameraController(const librii::math::AABB& bounding_box);
  void setGlWireframe(bool wireframe) const;
  void clearGlScreen() const;

private:
  // Scene state
  lib3d::SceneState mSceneState;

  lib3d::IDrawable* mRoot = nullptr;
  CameraController mCameraController;
  CameraController::ControllerType combo_choice_cam =
      CameraController::ControllerType::WASD_Minecraft;

  // Render settings
  bool rend = true;
#ifdef RII_NATIVE_GL_WIREFRAME
  bool wireframe = false;
#endif
};
} // namespace riistudio::frontend
