#pragma once

#include <cstdint>

#include "dxso_common.h"

namespace dxvk {

  /**
   * \brief Returns the length of d3d9 shader bytecode in bytes.
   */
  size_t DXSOBytecodeLength(const uint32_t* pFunction);

  enum class DxsoBindingType : uint32_t {
    ConstantBuffer,
    Image
  };

  enum DxsoConstantBuffers : uint32_t {
    VSConstantBuffer = 0,
    VSClipPlanes     = 1,
    VSFixedFunction  = 2,
    VSCount,

    PSConstantBuffer = 0,
    PSRenderStates   = 1,
    PSFixedFunction  = 2,
    PSCount
  };

  uint32_t computeResourceSlotId(
          DxsoProgramType shaderStage,
          DxsoBindingType bindingType,
          uint32_t        bindingIndex);

}