#pragma once

#include <librii/g3d/data/AnimData.hpp>
#include <librii/g3d/io/CommonIO.hpp>
#include <librii/g3d/io/DictIO.hpp>
#include <librii/g3d/io/DictWriteIO.hpp>
#include <librii/g3d/io/NameTableIO.hpp>
#include <map>
#include <oishii/reader/binary_reader.hxx>
#include <oishii/writer/binary_writer.hxx>
#include <rsl/SafeReader.hpp>
#include <string>
#include <variant>
#include <vector>

namespace librii::g3d {

struct SRT0KeyFrame {
  f32 frame{};
  f32 value{};
  f32 tangent{};
  bool operator==(const SRT0KeyFrame&) const = default;
};

struct SRT0Track {
  // There will be numFrames + 1 keyframes for some reason
  std::vector<SRT0KeyFrame> keyframes{};
  std::array<u8, 2> reserved{};
  f32 step{};

  bool operator==(const SRT0Track&) const = default;

  u32 computeSize() const;
  Result<void> read(rsl::SafeReader& safe);
  void write(oishii::Writer& writer) const;
};

struct SRT0Target {
  std::variant<f32, u32> data; // index to SRT0Track

  bool operator==(const SRT0Target&) const = default;
};

struct SRT0Matrix {
  enum Flag {
    FLAG_ENABLED = (1 << 0),
    FLAG_SCL_ONE = (1 << 1),
    FLAG_ROT_ZERO = (1 << 2),
    FLAG_TRANS_ZERO = (1 << 3),
    FLAG_SCL_ISOTROPIC = (1 << 4),
    FLAG_SCL_U_FIXED = (1 << 5),
    FLAG_SCL_V_FIXED = (1 << 6),
    FLAG_ROT_FIXED = (1 << 7),
    FLAG_TRANS_U_FIXED = (1 << 8),
    FLAG_TRANS_V_FIXED = (1 << 9),

    FLAG__COUNT = 10,
  };
  enum class TargetId {
    ScaleU,
    ScaleV,
    Rotate,
    TransU,
    TransV,
    Count,
  };
  static bool isFixed(TargetId target, u32 flags) {
    switch (target) {
    case TargetId::ScaleU:
      return flags & FLAG_SCL_U_FIXED;
    case TargetId::ScaleV:
      return flags & FLAG_SCL_V_FIXED;
    case TargetId::Rotate:
      return flags & FLAG_ROT_FIXED;
    case TargetId::TransU:
      return flags & FLAG_TRANS_U_FIXED;
    case TargetId::TransV:
      return flags & FLAG_TRANS_V_FIXED;
    case TargetId::Count:
      break; // Not a valid enum value
    }
    return false;
  }
  static bool isAttribIncluded(SRT0Matrix::TargetId attribute, u32 flags) {
    switch (attribute) {
    case TargetId::ScaleU:
      return (flags & FLAG_SCL_ONE) == 0;
    case TargetId::ScaleV:
      return (flags & FLAG_SCL_ISOTROPIC) == 0;
    case TargetId::Rotate:
      return (flags & FLAG_ROT_ZERO) == 0;
    case TargetId::TransU:
    case TargetId::TransV:
      return (flags & FLAG_TRANS_ZERO) == 0;
    case TargetId::Count:
      break; // Not a valid enum value
    }
    return false;
  }
  u32 flags{};

  std::vector<SRT0Target> targets; // Max 5 tracks - for each channel

  bool operator==(const SRT0Matrix&) const = default;

  u32 computeSize() const;
  Result<void> read(rsl::SafeReader& safe,
                    std::function<Result<u32>(u32)> trackAddressToIndex);
  void write(oishii::Writer& writer,
             std::function<u32(u32)> trackIndexToAddress) const;
};

struct SRT0Material {
  enum Flag {
    FLAG_ENABLED = (1 << 0),
  };
  std::string name{};
  u32 enabled_texsrts{};
  u32 enabled_indsrts{};
  // Max 8+3 matrices. These can't be merged and are placed
  // inline by the converter for some reason.
  std::vector<SRT0Matrix> matrices{};

  bool operator==(const SRT0Material&) const = default;

  u32 computeSize() const;
  Result<void> read(rsl::SafeReader& reader,
                    std::function<Result<u32>(u32)> trackAddressToIndex);
  void write(oishii::Writer& writer, NameTable& names,
             std::function<u32(u32)> trackIndexToAddress) const;
};

struct BinarySrt {
  std::vector<SRT0Material> materials;
  std::vector<SRT0Track> tracks;
  // TODO: User data
  std::string name;
  std::string sourcePath;
  u16 frameDuration{};
  u32 xformModel{};
  AnimationWrapMode wrapMode{AnimationWrapMode::Repeat};

  bool operator==(const BinarySrt&) const = default;

  Result<void> read(oishii::BinaryReader& reader);
  void write(oishii::Writer& writer, NameTable& names, u32 addrBrres) const;
};

// XML-suitable variant
struct SrtAnim {
  using Track = std::variant<f32, std::vector<SRT0KeyFrame>>;
  struct Mtx {
    Track scaleX{};
    Track scaleY{};
    Track rot{};
    Track transX{};
    Track transY{};
  };
  struct Mat {
    std::string name{};
    std::array<std::optional<Mtx>, 8> texsrts{};
    std::array<std::optional<Mtx>, 3> indsrts{};
  };
  std::vector<Mat> materials{};
  std::string name{};
  std::string sourcePath{};
  u16 frameDuration{};
  u32 xformModel{};
  AnimationWrapMode wrapMode{AnimationWrapMode::Repeat};

  static Result<SrtAnim> read(const BinarySrt& srt,
                              std::function<void(std::string_view)> warn) {
    SrtAnim tmp{};
    tmp.name = srt.name;
    tmp.sourcePath = srt.sourcePath;
    tmp.frameDuration = srt.frameDuration;
    tmp.xformModel = srt.xformModel;
    tmp.wrapMode = srt.wrapMode;
    for (auto& m : srt.materials) {
      auto& x = tmp.materials.emplace_back();
      x.name = m.name;
      size_t j = 0;
      for (size_t i = 0; i < 8; ++i) {
        if (m.enabled_texsrts & (1 << i)) {
          x.texsrts[i] = TRY(readMatrix(srt, m.matrices[j++], warn));
        }
      }
      for (size_t i = 0; i < 3; ++i) {
        if (m.enabled_indsrts & (1 << i)) {
          x.indsrts[i] = TRY(readMatrix(srt, m.matrices[j++], warn));
        }
      }
    }
    return tmp;
  }

private:
  static Result<Mtx> readMatrix(const BinarySrt& srt, const SRT0Matrix& mtx,
                                std::function<void(std::string_view)> warn) {
    size_t k = 0;
    Mtx y{};
    if (!mtx.isAttribIncluded(SRT0Matrix::TargetId::ScaleU, mtx.flags)) {
      y.scaleX = 1.0f;
    } else {
      y.scaleX = TRY(readTrack(srt.tracks, mtx.targets[k++], warn));
    }
    if (!mtx.isAttribIncluded(SRT0Matrix::TargetId::ScaleV, mtx.flags)) {
      y.scaleY = 1.0f;
    } else {
      y.scaleY = TRY(readTrack(srt.tracks, mtx.targets[k++], warn));
    }
    if (!mtx.isAttribIncluded(SRT0Matrix::TargetId::Rotate, mtx.flags)) {
      y.rot = 0.0f;
    } else {
      y.rot = TRY(readTrack(srt.tracks, mtx.targets[k++], warn));
    }
    if (!mtx.isAttribIncluded(SRT0Matrix::TargetId::TransU, mtx.flags)) {
      y.transX = 0.0f;
    } else {
      y.transX = TRY(readTrack(srt.tracks, mtx.targets[k++], warn));
    }
    if (!mtx.isAttribIncluded(SRT0Matrix::TargetId::TransV, mtx.flags)) {
      y.transY = 0.0f;
    } else {
      y.transY = TRY(readTrack(srt.tracks, mtx.targets[k++], warn));
    }
    return y;
  }
  static Result<Track> readTrack(std::span<const SRT0Track> tracks,
                                 const SRT0Target& target,
                                 std::function<void(std::string_view)> warn) {
    if (auto* fixed = std::get_if<f32>(&target.data)) {
      return Track{TRY(checkFloat(*fixed))};
    } else {
      assert(std::get_if<u32>(&target.data));
      auto& track = tracks[*std::get_if<u32>(&target.data)];
      EXPECT(track.reserved[0] == 0 && track.reserved[1] == 0);
      EXPECT(tracks.size() >= 1);
      for (auto& f : track.keyframes) {
        TRY(checkFloat(f.frame));
        TRY(checkFloat(f.value));
        TRY(checkFloat(f.tangent));
      }
      auto strictly_increasing =
          std::ranges::adjacent_find(track.keyframes, [](auto& x, auto& y) {
            return x.frame >= y.frame;
          }) == track.keyframes.end();
      EXPECT(strictly_increasing,
             "SRT0 track keyframes must be strictly increasing");
      f32 begin = track.keyframes[0].frame;
      f32 end = track.keyframes.back().frame;
      auto step = CalcStep(begin, end);
      if (track.step != step) {
        warn("Frame interval not properly computed");
      }
      return Track{track.keyframes};
    }
  }

  static Result<f32> checkFloat(f32 in) {
    if (std::isinf(in)) {
      return in > 0.0f ? std::unexpected("Float is set to INFINITY")
                       : std::unexpected("Float is set to -INFINITY");
    }
    if (std::isnan(in)) {
      return in > 0.0f ? std::unexpected("Float is set to NAN")
                       : std::unexpected("Float is set to -NAN");
    }
    return in;
  }
};

using SrtAnimationArchive = BinarySrt;

} // namespace librii::g3d
