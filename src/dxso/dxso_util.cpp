#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  size_t DXSOBytecodeLength(const uint32_t* pFunction) {
    const uint32_t* start = reinterpret_cast<const uint32_t*>(pFunction);
    const uint32_t* current = start;

    while (*current != 0x0000ffff) // A token of 0x0000ffff indicates the end of the bytecode.
      current++;

    return size_t(current - start) * sizeof(uint32_t);
  }

  uint32_t computeResourceSlotId(
        DxsoProgramType shaderStage,
        DxsoBindingType bindingType,
        uint32_t        bindingIndex) {
    const uint32_t stageOffset = 6 * uint32_t(shaderStage);

    if (shaderStage == DxsoProgramType::VertexShader) {
      switch (bindingType) {
        case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0; // 0 + 3 = 3
        case DxsoBindingType::Image:          return bindingIndex + stageOffset + 3; // 3 + 4 = 6
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }
    else { // Pixel Shader
      switch (bindingType) {
      case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0;  // 0 + 3 = 3
        // The extra sampler here is being reserved for DMAP stuff later on.
      case DxsoBindingType::Image:          return bindingIndex + stageOffset + 3;  // 3 + 17 = 19
      default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }

    return 0;
  }

}