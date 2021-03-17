#include "root.hpp"
#include <core/3d/gl.hpp>
#include <core/api.hpp>
#include <core/util/gui.hpp>
#include <core/util/timestamp.hpp>
#include <frontend/editor/EditorWindow.hpp>
#include <frontend/widgets/fps.hpp>
#include <frontend/widgets/fullscreen.hpp>
#include <frontend/widgets/theme_editor.hpp>
#include <fstream>
#include <imgui_markdown.h>
#include <oishii/reader/binary_reader.hxx>
#include <oishii/writer/binary_writer.hxx>
#include <pfd/portable-file-dialogs.h>
// Experimental conversion
#include <plugins/g3d/collection.hpp>
#include <plugins/j3d/Scene.hpp>

#include <vendor/stb_image.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef BUILD_ASAN
#include <sanitizer/lsan_interface.h>
#endif

namespace riistudio::frontend {

RootWindow* RootWindow::spInstance;

#ifdef _WIN32
static void GlCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                       GLsizei length, const GLchar* message,
                       GLvoid* userParam) {
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    return;

  printf("%s\n", message);
}
#endif

void RootWindow::draw() {
  fileHostProcess();

  ImGui::PushID(0);
  if (BeginFullscreenWindow("##RootWindow", getOpen())) {
    if (mThemeUpdated) {
      mTheme.setThemeEx(mCurTheme);
      mThemeUpdated = false;
    }
    ImGui::GetIO().FontGlobalScale = mFontGlobalScale;

    ImGui::SetWindowFontScale(1.1f);
    if (!hasChildren()) {
      ImGui::Text("Drop a file to edit.");
    }
    // ImGui::Text(
    //     "Note: For this early alpha, file saving is temporarily disabled.");
    ImGui::SetWindowFontScale(1.0f);
    dockspace_id = ImGui::GetID("DockSpaceWidget");

    // ImGuiID dock_main_id = dockspace_id;
    while (mAttachEditorsQueue.size()) {
      const std::string& ed_id = mAttachEditorsQueue.front();
      (void)ed_id;

      // ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing
      // layout ImGui::DockBuilderAddNode(dockspace_id); // Add empty node
      //
      //
      // ImGuiID dock_up_id = ImGui::DockBuilderSplitNode(dock_main_id,
      // ImGuiDir_Up, 0.05f, nullptr, &dock_main_id);
      // ImGui::DockBuilderDockWindow(ed_id.c_str(), dock_up_id);
      //
      //
      // ImGui::DockBuilderFinish(dockspace_id);
      mAttachEditorsQueue.pop();
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), 0);

    EditorWindow* ed =
        getActive() ? dynamic_cast<EditorWindow*>(getActive()) : nullptr;
    assert(ed || !hasChildren());

    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
#ifdef _WIN32
        if (ImGui::MenuItem("Open")) {
          openFile();
        }
#endif
        if (ImGui::MenuItem("Save")) {

          if (ed) {
            DebugReport("Attempting to save to %s\n",
                        ed->getFilePath().c_str());
            if (!ed->getFilePath().empty())
              save(ed->getFilePath());
            else
              saveAs();
          } else {
            printf("Cannot save.. nothing has been opened.\n");
          }
        }
#ifdef _WIN32
        if (ImGui::MenuItem("Save As")) {
          if (ed)
            saveAs();
          else
            printf("Cannot save.. nothing has been opened.\n");
        }
#endif
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Windows")) {
        //	if (ed && ed->mWindowCollection)
        //	{
        //		for (int i = 0; i < ed->mWindowCollection->getNum();
        //++i)
        //		{
        //			if
        //(ImGui::MenuItem(ed->mWindowCollection->getName(i)))
        //			{
        //				ed->attachWindow(ed->mWindowCollection->spawn(i),
        // ed->mId);
        //			}
        //		}
        //	}
        ed = nullptr;
        ed->attachWindow(nullptr);
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Settings")) {
        bool _vsync = vsync;
        ImGui::Checkbox("VSync", &_vsync);

        if (_vsync != vsync) {
          setVsync(_vsync);
          vsync = _vsync;
        }

        mThemeUpdated |= DrawThemeEditor(mCurTheme, mFontGlobalScale, nullptr);

#ifdef BUILD_DEBUG
        ImGui::Checkbox("ImGui Demo", &bDemo);
#endif

        ImGui::EndMenu();
      }

      if (bDemo)
        ImGui::ShowDemoWindow(&bDemo);

#ifndef BUILD_DIST
      if (ImGui::BeginMenu("Experimental")) {
        if (ImGui::MenuItem("Convert to BMD") && ed != nullptr) {
          const libcube::Scene* from_root =
              dynamic_cast<libcube::Scene*>(&ed->getDocument().getRoot());
          auto from_models = from_root->getModels();
          auto from_textures = from_root->getTextures();

          std::unique_ptr<kpi::INode> bmd_state{dynamic_cast<kpi::INode*>(
              SpawnState(typeid(j3d::Collection).name()).release())};

          j3d::Collection& bmd_col =
              *dynamic_cast<j3d::Collection*>(bmd_state.get());

          for (auto& from_model : from_models) {
            auto& bmd_model = bmd_col.getModels().add();
            // info
            // Bufs (automate)
            bmd_model.mBufs.norm.mQuant.comp.normal =
                librii::gx::VertexComponentCount::Normal::xyz;
            // drawmtx (skip)
            auto& mtx = bmd_model.mDrawMatrices.emplace_back();
            mtx.mWeights.emplace_back(0, 1.0f);
            // materials
            {
              const auto from_mats = from_model.getMaterials();
              for (int m_i = 0; m_i < from_mats.size(); ++m_i) {
                auto& mat = bmd_model.getMaterials().add();

                mat.getMaterialData() = from_mats[m_i].getMaterialData();

                mat.indEnabled = !mat.getMaterialData().indirectStages.empty();
              }
            }
            // joints
            {
              auto from_joints = from_model.getBones();
              for (int m_i = 0; m_i < from_joints.size(); ++m_i) {
                auto& joint = bmd_model.getBones().add();
                joint.id = m_i;
                from_joints[m_i].copy(joint);
              }
            }
            // shapes
            {
              auto from_shapes = from_model.getMeshes();
              for (int m_i = 0; m_i < from_shapes.size(); ++m_i) {
                auto& from_shape = from_shapes[m_i];
                const auto vcd = from_shape.getVcd();

                auto& bmd_shape = bmd_model.getMeshes().add();
                bmd_shape.id = m_i;
                bmd_shape.mVertexDescriptor = vcd;
                for (auto& e : bmd_shape.mVertexDescriptor.mAttributes) {
                  e.second = librii::gx::VertexAttributeType::Short;
                }
                bmd_shape.mVertexDescriptor
                    .calcVertexDescriptorFromAttributeList();

                for (int i = 0;
                     i < from_shape.getMeshData().mMatrixPrimitives.size();
                     ++i) {
                  auto& bmd_mp = bmd_shape.mMatrixPrimitives.emplace_back();
                  bmd_mp.mCurrentMatrix = 0;
                  bmd_mp.mDrawMatrixIndices.push_back(0);
                  // No multi mtx yet
                  for (auto& prim : from_shape.getMeshData()
                                        .mMatrixPrimitives[i]
                                        .mPrimitives) {
                    auto& p = bmd_mp.mPrimitives.emplace_back(prim);
                    // Remap vtx indices
                    for (auto& v : p.mVertices) {
                      for (u32 x = 0; x < (u32)librii::gx::VertexAttribute::Max;
                           ++x) {
                        if (!(vcd.mBitfield & (1 << x)))
                          continue;
                        auto& bufs = bmd_model.mBufs;
                        switch (static_cast<librii::gx::VertexAttribute>(x)) {
                        case librii::gx::VertexAttribute::
                            PositionNormalMatrixIndex:
                        case librii::gx::VertexAttribute::Texture0MatrixIndex:
                        case librii::gx::VertexAttribute::Texture1MatrixIndex:
                        case librii::gx::VertexAttribute::Texture2MatrixIndex:
                        case librii::gx::VertexAttribute::Texture3MatrixIndex:
                        case librii::gx::VertexAttribute::Texture4MatrixIndex:
                        case librii::gx::VertexAttribute::Texture5MatrixIndex:
                        case librii::gx::VertexAttribute::Texture6MatrixIndex:
                        case librii::gx::VertexAttribute::Texture7MatrixIndex:
                          break;
                        case librii::gx::VertexAttribute::Position: {
                          auto pos = from_shape.getPos(
                              from_model,
                              v[librii::gx::VertexAttribute::Position]);
                          auto found = std::find(bufs.pos.mData.begin(),
                                                 bufs.pos.mData.end(), pos);
                          if (found == bufs.pos.mData.end()) {
                            bmd_model.mBufs.pos.mData.push_back(pos);
                            v[librii::gx::VertexAttribute::Position] =
                                bmd_model.mBufs.pos.mData.size() - 1;
                          } else {
                            v[librii::gx::VertexAttribute::Position] =
                                found - bufs.pos.mData.begin();
                          }
                          break;
                        }
                        case librii::gx::VertexAttribute::Color0: {
                          const auto scolor = from_shape.getClr(
                              from_model, 0,
                              v[librii::gx::VertexAttribute::Color0]);
                          librii::gx::Color clr;
                          clr.r = roundf(scolor[0] * 255.0f);
                          clr.g = roundf(scolor[1] * 255.0f);
                          clr.b = roundf(scolor[2] * 255.0f);
                          clr.a = roundf(scolor[3] * 255.0f);
                          bmd_model.mBufs.color[0].mData.push_back(clr);
                          v[librii::gx::VertexAttribute::Color0] =
                              bmd_model.mBufs.color[0].mData.size() - 1;
                          break;
                        }
                        case librii::gx::VertexAttribute::TexCoord0:
                        case librii::gx::VertexAttribute::TexCoord1:
                        case librii::gx::VertexAttribute::TexCoord2:
                        case librii::gx::VertexAttribute::TexCoord3:
                        case librii::gx::VertexAttribute::TexCoord4:
                        case librii::gx::VertexAttribute::TexCoord5:
                        case librii::gx::VertexAttribute::TexCoord6:
                        case librii::gx::VertexAttribute::TexCoord7: {
                          const auto chan =
                              x - static_cast<int>(
                                      librii::gx::VertexAttribute::TexCoord0);
                          const auto attr =
                              static_cast<librii::gx::VertexAttribute>(x);
                          const auto data =
                              from_shape.getUv(from_model, chan, v[attr]);

                          bmd_model.mBufs.uv[chan].mData.push_back(data);
                          v[static_cast<librii::gx::VertexAttribute>(x)] =
                              bmd_model.mBufs.uv[chan].mData.size() - 1;

                          break;
                        }
                        case librii::gx::VertexAttribute::Normal:
                          bmd_model.mBufs.norm.mData.push_back(
                              from_shape.getNrm(
                                  from_model,
                                  v[librii::gx::VertexAttribute::Normal]));
                          v[librii::gx::VertexAttribute::Normal] =
                              bmd_model.mBufs.norm.mData.size() - 1;
                          break;
                        default:
                          assert(!"Invalid vtx attrib");

                          break;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          // textures
          for (const auto& from_texture : from_textures) {
            auto& bt = bmd_col.getTextures().add();
            bt.mName = from_texture.getName();
            bt.mFormat = from_texture.getTextureFormat();
            bt.bTransparent = false; // TODO
            bt.mWidth = from_texture.getWidth();
            bt.mHeight = from_texture.getHeight();
            bt.mPaletteFormat = from_texture.getPaletteFormat();
            bt.nPalette = 0;
            bt.ofsPalette = 0;
            bt.mMinLod = 0; // TODO: Verify
            bt.mMaxLod = from_texture.getMipmapCount() + 1;
            bt.mMipmapLevel = from_texture.getMipmapCount() + 1;
            bt.mData.resize(from_texture.getEncodedSize(true));
            memcpy(bt.mData.data(), from_texture.getData(), bt.mData.size());
            // TODO: Copy
            // bmd_texture.get()
          }

          attachEditorWindow(std::make_unique<EditorWindow>(
              std::move(bmd_state), "__conv.bmd"));
        }
        ImGui::EndMenu();
      }
#endif

      ImGui::SameLine(ImGui::GetWindowWidth() - 60);
      DrawFps();

      ImGui::EndMenuBar();
    }

    if (mCheckUpdate)
      mUpdater.draw();

    if (!mImportersQueue.empty()) {
      auto& window = mImportersQueue.front();
      if (window.attachEditor()) {
        attachEditorWindow(std::make_unique<EditorWindow>(window.takeResult(),
                                                          window.getPath()));
        mImportersQueue.pop();
      } else if (window.abort()) {
        mImportersQueue.pop();
      } else {
        const auto wflags = ImGuiWindowFlags_NoCollapse;

        ImGui::OpenPopup("Importer");

        ImGui::SetNextWindowSize({800.0f, 00.0f});
        ImGui::BeginPopupModal("Importer", nullptr, wflags);
        window.draw();
        ImGui::EndPopup();
      }
    }
    drawChildren();
  }
  // Handle popups
  EndFullscreenWindow();
  ImGui::PopID();

#ifdef BUILD_ASAN
  __lsan_do_leak_check();
#endif
}
void RootWindow::onFileOpen(FileData data, OpenFilePolicy policy) {
  printf("Opening file: %s\n", data.mPath.c_str());

  if (mWantFile) {
    mReqData = std::move(data);
    mGotFile = true;
    return;
  }

  if (!mImportersQueue.empty()) {
    auto& top = mImportersQueue.front();

    if (top.acceptDrop()) {
      top.drop(std::move(data));
      return;
    }
  }

  mImportersQueue.emplace(std::move(data));
}
void RootWindow::attachEditorWindow(std::unique_ptr<EditorWindow> editor) {
  mAttachEditorsQueue.push(editor->getName());
  attachWindow(std::move(editor));
}

RootWindow::RootWindow() : Applet(std::string("RiiStudio ") + RII_TIME_STAMP) {
#ifdef _WIN32
  glDebugMessageCallback(GlCallback, 0);
#endif
  spInstance = this;

  InitAPI();

  ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
#ifdef RII_BACKEND_GLFW
  GLFWwindow* window = reinterpret_cast<GLFWwindow*>(getPlatformWindow());
  GLFWimage image;
  image.pixels = stbi_load("icon.png", &image.width, &image.height, 0, 4);
  glfwSetWindowIcon(window, 1, &image);
  stbi_image_free(image.pixels);
#endif
}
RootWindow::~RootWindow() { DeinitAPI(); }

void RootWindow::save(const std::string& path) {
  EditorWindow* ed =
      getActive() ? dynamic_cast<EditorWindow*>(getActive()) : nullptr;
  if (ed != nullptr)
    ed->saveAs(path);
}
void RootWindow::saveAs() {
  std::vector<std::string> filters;

  EditorWindow* ed =
      getActive() ? dynamic_cast<EditorWindow*>(getActive()) : nullptr;
  if (ed == nullptr)
    return;

  const kpi::INode* node = &ed->getDocument().getRoot();

  if (dynamic_cast<const riistudio::j3d::Collection*>(node) != nullptr) {
    filters.push_back("Binary Model Data (*.bmd)");
    filters.push_back("*.bmd");
  } else if (dynamic_cast<const riistudio::g3d::Collection*>(node) != nullptr) {
    filters.push_back("Binary Resource (*.brres)");
    filters.push_back("*.brres");
  }

  filters.push_back("All Files");
  filters.push_back("*");

  auto results = pfd::save_file("Save File", "", filters).result();
  if (results.empty())
    return;

  if (dynamic_cast<const riistudio::j3d::Collection*>(node) != nullptr) {
    if (!results.ends_with(".bmd"))
      results.append(".bmd");
  } else if (dynamic_cast<const riistudio::g3d::Collection*>(node) != nullptr) {
    if (!results.ends_with(".brres"))
      results.append(".brres");
  }

  save(results);
}

} // namespace riistudio::frontend
