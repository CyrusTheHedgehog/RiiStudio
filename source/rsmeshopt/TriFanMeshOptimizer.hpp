#pragma once

#include <core/common.h>

#include "MeshUtils.hpp"
#include "RingIterator.hpp"
#include "TriangleFanSplitter.hpp"

namespace rsmeshopt {

// Options for TriFanMeshOptimizer
struct TriFanOptions {
  // Should never go lower, as a 3-triangle fan is also a strip.
  u32 min_fan_size = 4;
  //
  size_t max_runs{std::numeric_limits<size_t>::max()};
};

// Class that generates triangle fans from a provided indexed triangle mesh
// array. The fans represent a more memory-efficient storage of triangle
// connectivity that can be used directly on the GPU. (See
// https://en.wikipedia.org/wiki/Triangle_fan). Usually, triangle strips are
// more versaitile representations of a scene, so algorithm will output trifans
// if there is a really good topological match (fan of 4). This class is
// intended to be used as a pre-pass before triangle stripping; it is very
// unlikely an entire mesh can be well-described as triangle fans.
//
// This class's public API is based on draco::MeshStripifier.
//
// The algorithm TriFanPass uses follows:
//
// 1) Compute degree of all vertices.
// 2) Starting with the highest degree vertex: (this will be the candidate
// "center" vertex)
//    A) Collect all adjacent triangles and sequester them into "islands" of
//    connectivity by the non-center vetex
//       -> for each existing island, allow an additional triangle to be added
//       iff there is a non-center connecting vertex with subgraph degree 2 (not
//       connected to an additional triangle)
//    B) Discard islands with #triangles < some threshhold (4 usually).
//    C) Dor each island, extract all edges not containing center and perform a
//    topological sort. Output this as a triangle fan.
//    D) Mark these triangles as visited and adjust vertex degree cache.
//    E) Loop to the next highest degree vertex if possible.
// 3) For all non-visited triangles, output as simple triangles.
//
// Some flaws with the algorithm:
//   - A vertex having a high degree doesn't necessarily mean it will begin the
//   longest triangle fan. Winding order and odd topology can cause false
//   positives to appear. We are potentially "stealing" triangles from larger
//   fans when we encounter false postiives. Notwithstanding, this heuristic
//   seems to work really well in practice; usually fans are distinct objects in
//   the scene and are unlikely to compete for resources.
//
//   - Perf: When constructing a fan from an island, we invoke the very general
//   RingIterator which assumes nothing and performs a topological sort. This
//   may be uncessary, as we've already calculated connectivity information when
//   determining if it is valid to append to a strip.
//
//   - We have no insight into the triangle stripping post-pass that will
//   follow. A more complete approach that combines triangle strips and fans may
//   better be able to find the global minimum.
//
class TriFanMeshOptimizer {
public:
  TriFanMeshOptimizer() = default;
  ~TriFanMeshOptimizer() = default;

  // Generate triangle fans for a given mesh and output them to the output
  // iterator |out_it|. In most cases |out_it| stores the values in a buffer
  // that can be used directly on the GPU. Note that the algorithm can generate
  // multiple fans to represent the whole mesh. In such cases multiple strips
  // are separated using a so-called primitive restart index, specified by
  // |primtive_restart_index| (usually defined as the maximum allowed value for
  // the given type).
  // https://www.khronos.org/opengl/wiki/Vertex_Rendering#Primitive_Restart
  template <typename OutputIteratorT, typename IndexTypeT>
  bool GenerateTriangleFansWithPrimitiveRestart(
      std::span<const u32> mesh, IndexTypeT primitive_restart_index,
      OutputIteratorT out_it, TriFanOptions options = {});

  // Returns the number of fans generated by the last call of the
  // GenerateTriangleFansWithPrimitiveRestart() methods.
  int num_fans() const { return num_fans_; }

private:
  bool Prepare(std::span<const u32> mesh, const TriFanOptions& options) {
    assert(mesh.size() % 3 == 0);
    if (MeshUtils::TriangleArrayHoldsDuplicates(mesh)) {
      return false;
    }
    mesh_ = mesh;
    face_visited_.clear();
    face_visited_.resize(mesh.size() / 3);
    std::set<u32> verts(mesh.begin(), mesh.end());
    num_vertices_ = verts.size();
    valence_cache_.clear();
    valence_cache_.resize(num_vertices_);
    for (u32 vertex : mesh_) {
      ++valence_cache_[vertex];
    }
    min_fan_size_ = options.min_fan_size;
    max_runs_ = options.max_runs;
    return true;
  }

  template <typename OutputIteratorT>
  bool StoreFan(size_t center, std::span<const u32> fan,
                OutputIteratorT out_it) {
    ++num_fans_;
    std::vector<size_t> island_;
    for (size_t face : fan) {
      face_visited_[face / 3] = true;
      island_.push_back(mesh_[face]);
      island_.push_back(mesh_[face + 1]);
      island_.push_back(mesh_[face + 2]);
    }

    // Update valence cache
    for (size_t vert : island_) {
      // For centers this can become negative
      if (valence_cache_[vert] == 0) {
        continue;
      }
      --valence_cache_[vert];
    }

    // TODO: Debugging: Enable to skip RingIterator and just assess islands
    *out_it++ = center;
    [[maybe_unused]] size_t num_verts = 1;

    RingIterator<size_t> ring_iterator(center, island_);
    if (!ring_iterator.valid()) {
      return false;
    }
    for (auto vtx : ring_iterator) {
      *out_it++ = vtx;
      ++num_verts;
    }
    assert(num_verts == island_.size() / 3 + 2);
    return true;
  }

  std::vector<std::vector<u32>>
  FindFansFromCenter(u32 center, u32 primitive_restart_index, auto&& out_it) {
    // Break candidates up into islands sharing at least two vertices.
    TriangleFanSplitter splitter;
    auto islands = splitter.ConvertToFans(mesh_, center);

    std::vector<std::vector<u32>> fans;
    for (auto& island : islands) {
      // Only care about islands >= 4 in length (3 just becomes a strip)
      if (island.size() < min_fan_size_) {
        continue;
      }
      std::vector<u32> tmp(island.begin(), island.end());
      fans.push_back(tmp);
    }
    return fans;
  }

  std::span<const u32> mesh_{};
  int num_vertices_{};
  std::vector<bool> face_visited_{};

  // Holds twice the degree of each vertex N
  std::vector<int> valence_cache_{};
  int num_fans_{};

  // Options
  int min_fan_size_{};
  size_t max_runs_{std::numeric_limits<size_t>::max()};
};

template <typename OutputIteratorT, typename IndexTypeT>
bool TriFanMeshOptimizer::GenerateTriangleFansWithPrimitiveRestart(
    std::span<const u32> mesh, IndexTypeT primitive_restart_index,
    OutputIteratorT out_it, TriFanOptions options) {
  if (!Prepare(mesh, options)) {
    return false;
  }

  size_t num_runs = std::min<size_t>(num_vertices_, max_runs_);
  for (size_t i = 0; i < num_runs; ++i) {
    auto max = std::max_element(valence_cache_.begin(), valence_cache_.end());
    assert(max != valence_cache_.end());
    size_t center = max - valence_cache_.begin();
    // We've considered every vertex
    if (*max < min_fan_size_) {
      break;
    }
    auto fans = FindFansFromCenter(center, primitive_restart_index, out_it);
    for (auto& fan : fans) {
      if (!StoreFan(center, fan, out_it)) {
        return false;
      }
      *out_it++ = primitive_restart_index;
    }
    // Effectively kill this vertex from our subgraph moving forward
    valence_cache_[center] = 0;
  }

  // Output remaining triangles in bulk
  for (size_t i = 0; i < face_visited_.size(); ++i) {
    if (!face_visited_[i]) {
      *out_it++ = mesh[i * 3];
      *out_it++ = mesh[i * 3 + 1];
      *out_it++ = mesh[i * 3 + 2];
      *out_it++ = primitive_restart_index;
    }
  }

  return true;
}

} // namespace rsmeshopt
