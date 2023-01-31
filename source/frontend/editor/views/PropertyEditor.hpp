#pragma once

#include <LibBadUIFramework/History.hpp>             // kpi::History
#include <LibBadUIFramework/Node2.hpp>               // kpi::IDocumentNode
#include <LibBadUIFramework/PropertyView.hpp>        // PropertyViewStateHolder
#include <frontend/editor/EditorWindow.hpp> // EditorWindow
#include <frontend/editor/StudioWindow.hpp> // StudioWindow

namespace riistudio::frontend {

std::unique_ptr<StudioWindow> MakePropertyEditor(kpi::History& host,
                                                 kpi::INode& root,
                                                 SelectionManager& active,
                                                 EditorWindow& ed);

} // namespace riistudio::frontend
