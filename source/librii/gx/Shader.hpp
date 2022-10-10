#pragma once

#include "Indirect.hpp"
#include <array>
#include <core/common.h>
#include <rsl/SmallVector.hpp>
#include <vector>

namespace librii::gx {

enum class TevColorArg {
  cprev,
  aprev,
  c0,
  a0,
  c1,
  a1,
  c2,
  a2,
  texc,
  texa,
  rasc,
  rasa,
  one,
  half,
  konst,
  zero
};

enum class TevAlphaArg { aprev, a0, a1, a2, texa, rasa, konst, zero };

enum class TevBias {
  zero,     //!< As-is
  add_half, //!< Add middle gray
  sub_half  //!< Subtract middle gray
};

enum class TevReg {
  prev,
  reg0,
  reg1,
  reg2,

  reg3 = prev
};

enum class TevColorOp {
  add,
  subtract,

  comp_r8_gt = 8,
  comp_r8_eq,
  comp_gr16_gt,
  comp_gr16_eq,
  comp_bgr24_gt,
  comp_bgr24_eq,
  comp_rgb8_gt,
  comp_rgb8_eq
};

enum class TevScale { scale_1, scale_2, scale_4, divide_2 };

enum class TevAlphaOp {
  add,
  subtract,

  comp_r8_gt = 8,
  comp_r8_eq,
  comp_gr16_gt,
  comp_gr16_eq,
  comp_bgr24_gt,
  comp_bgr24_eq,
  // Different from ColorOp
  comp_a8_gt,
  comp_a8_eq
};

enum class ColorComponent { r = 0, g, b, a };

enum class TevKColorSel {
  const_8_8,
  const_7_8,
  const_6_8,
  const_5_8,
  const_4_8,
  const_3_8,
  const_2_8,
  const_1_8,

  k0 = 12,
  k1,
  k2,
  k3,
  k0_r,
  k1_r,
  k2_r,
  k3_r,
  k0_g,
  k1_g,
  k2_g,
  k3_g,
  k0_b,
  k1_b,
  k2_b,
  k3_b,
  k0_a,
  k1_a,
  k2_a,
  k3_a
};

enum class TevKAlphaSel {
  const_8_8,
  const_7_8,
  const_6_8,
  const_5_8,
  const_4_8,
  const_3_8,
  const_2_8,
  const_1_8,

  // Not valid. For generic code
  // {
  k0 = 12,
  k1,
  k2,
  k3,
  // }

  k0_r = 16,
  k1_r,
  k2_r,
  k3_r,
  k0_g,
  k1_g,
  k2_g,
  k3_g,
  k0_b,
  k1_b,
  k2_b,
  k3_b,
  k0_a,
  k1_a,
  k2_a,
  k3_a
};

enum class ColorSelChanLow {
  color0a0,
  color1a1,

  ind_alpha = 5,
  normalized_ind_alpha, // ind_alpha in range [0, 255]
  null                  // zero
};
enum class ColorSelChanApi {
  color0,
  color1,
  alpha0,
  alpha1,
  color0a0,
  color1a1,
  zero,

  ind_alpha,
  normalized_ind_alpha,
  null = 0xFF
};

struct TevStage {
  // RAS1_TREF
  ColorSelChanApi rasOrder = ColorSelChanApi::null;
  u8 texMap = 0;
  u8 texCoord = 0;
  u8 rasSwap = 0;
  u8 texMapSwap = 0;

  struct ColorStage {
    // KSEL
    TevKColorSel constantSelection = TevKColorSel::k0;
    // COLOR_ENV
    TevColorArg a = TevColorArg::zero;
    TevColorArg b = TevColorArg::zero;
    TevColorArg c = TevColorArg::zero;
    TevColorArg d = TevColorArg::cprev;
    TevColorOp formula = TevColorOp::add;
    TevBias bias = TevBias::zero;
    TevScale scale = TevScale::scale_1;
    bool clamp = true;
    TevReg out = TevReg::prev;

    bool operator==(const ColorStage& rhs) const noexcept = default;
  } colorStage;
  struct AlphaStage {
    TevAlphaArg a = TevAlphaArg::zero;
    TevAlphaArg b = TevAlphaArg::zero;
    TevAlphaArg c = TevAlphaArg::zero;
    TevAlphaArg d = TevAlphaArg::aprev;
    TevAlphaOp formula = TevAlphaOp::add;
    // KSEL
    TevKAlphaSel constantSelection = TevKAlphaSel::k0_a;
    TevBias bias = TevBias::zero;
    TevScale scale = TevScale::scale_1;
    bool clamp = true;
    TevReg out = TevReg::prev;

    bool operator==(const AlphaStage& rhs) const noexcept = default;
  } alphaStage;
  struct IndirectStage {
    u8 indStageSel{0}; // TODO: Ind tex stage sel
    IndTexFormat format{IndTexFormat::_8bit};
    IndTexBiasSel bias{IndTexBiasSel::none};
    IndTexMtxID matrix{IndTexMtxID::off};
    IndTexWrap wrapU{IndTexWrap::off};
    IndTexWrap wrapV{IndTexWrap::off};

    bool addPrev{false};
    bool utcLod{false};
    IndTexAlphaSel alpha{IndTexAlphaSel::off};

    bool operator==(const IndirectStage& stage) const noexcept = default;
  } indirectStage;

  bool operator==(const TevStage& rhs) const noexcept = default;
};

// SWAP table
struct SwapTableEntry {
  ColorComponent r = ColorComponent::r, g = ColorComponent::g,
                 b = ColorComponent::b, a = ColorComponent::a;

  bool operator==(const SwapTableEntry& rhs) const noexcept = default;

  ColorComponent lookup(ColorComponent other) const {
    switch (other) {
    case ColorComponent::r:
      return r;
    case ColorComponent::g:
      return g;
    case ColorComponent::b:
      return b;
    case ColorComponent::a:
      return a;
    }
  }
};

struct SwapTable : public std::array<SwapTableEntry, 4> {
  using Clr = ColorComponent;
  SwapTable() {
    (*this)[0] = {Clr::r, Clr::g, Clr::b, Clr::a};
    (*this)[1] = {Clr::r, Clr::r, Clr::r, Clr::a};
    (*this)[2] = {Clr::g, Clr::g, Clr::g, Clr::a};
    (*this)[3] = {Clr::b, Clr::b, Clr::b, Clr::a};
  }
};

} // namespace librii::gx
