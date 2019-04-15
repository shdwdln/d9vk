#include "d3d9_util.h"

namespace dxvk {

  typedef HRESULT (STDMETHODCALLTYPE *D3DXDisassembleShader) (
    const void*      pShader, 
          BOOL       EnableColorCode, 
          char*      pComments, 
          ID3DBlob** ppDisassembly); // ppDisassembly is actually a D3DXBUFFER, but it has the exact same vtable as a ID3DBlob at the start.

  D3DXDisassembleShader g_pfnDisassembleShader = nullptr;

  HRESULT DisassembleShader(
    const void*      pShader, 
          BOOL       EnableColorCode, 
          char*      pComments, 
          ID3DBlob** ppDisassembly) {
    if (g_pfnDisassembleShader == nullptr) {
      HMODULE d3d9x = LoadLibraryA("d3dx9.dll");

      if (d3d9x == nullptr)
        d3d9x = LoadLibraryA("d3dx9_43.dll");

      g_pfnDisassembleShader = 
        reinterpret_cast<D3DXDisassembleShader>(GetProcAddress(d3d9x, "D3DXDisassembleShader"));
    }

    if (g_pfnDisassembleShader == nullptr)
      return D3DERR_INVALIDCALL;

    return g_pfnDisassembleShader(
      pShader,
      EnableColorCode,
      pComments,
      ppDisassembly);
  }

  HRESULT DecodeMultiSampleType(
        D3DMULTISAMPLE_TYPE       MultiSample,
        VkSampleCountFlagBits*    pCount) {
    VkSampleCountFlagBits flag;

    switch (MultiSample) {
    case D3DMULTISAMPLE_NONE:
    case D3DMULTISAMPLE_NONMASKABLE: flag = VK_SAMPLE_COUNT_1_BIT;  break;
    case D3DMULTISAMPLE_2_SAMPLES:   flag = VK_SAMPLE_COUNT_2_BIT;  break;
    case D3DMULTISAMPLE_4_SAMPLES:   flag = VK_SAMPLE_COUNT_4_BIT;  break;
    case D3DMULTISAMPLE_8_SAMPLES:   flag = VK_SAMPLE_COUNT_8_BIT;  break;
    case D3DMULTISAMPLE_16_SAMPLES:  flag = VK_SAMPLE_COUNT_8_BIT; break;
    default:                         return D3DERR_INVALIDCALL;
    }

    if (pCount != nullptr)
      *pCount = flag;

    return D3D_OK;
  }

  bool    ResourceBindable(
      DWORD                     Usage,
      D3DPOOL                   Pool) {
    return true;
  }

  VkFormat GetPackedDepthStencilFormat(D3D9Format Format) {
    switch (Format) {
    case D3D9Format::D15S1:
      return VK_FORMAT_D16_UNORM_S8_UINT; // This should never happen!

    case D3D9Format::D16:
    case D3D9Format::D16_LOCKABLE:
    case D3D9Format::DF16:
      return VK_FORMAT_D16_UNORM;

    case D3D9Format::D24X8:
    case D3D9Format::DF24:
      return VK_FORMAT_X8_D24_UNORM_PACK32;

    case D3D9Format::D24X4S4:
    case D3D9Format::D24FS8:
    case D3D9Format::D24S8:
    case D3D9Format::INTZ:
      return VK_FORMAT_D24_UNORM_S8_UINT;

    case D3D9Format::D32:
    case D3D9Format::D32_LOCKABLE:
    case D3D9Format::D32F_LOCKABLE:
      return VK_FORMAT_D32_SFLOAT;

    case D3D9Format::S8_LOCKABLE:
      return VK_FORMAT_S8_UINT;

    default:
      return VK_FORMAT_UNDEFINED;
    }
  }

  VkFormatFeatureFlags GetImageFormatFeatures(DWORD Usage) {
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    if (Usage & D3DUSAGE_DEPTHSTENCIL)
      features |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (Usage & D3DUSAGE_RENDERTARGET)
      features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

    return features;
  }

  VkImageUsageFlags GetImageUsageFlags(DWORD Usage) {
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (Usage & D3DUSAGE_DEPTHSTENCIL)
      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (Usage & D3DUSAGE_RENDERTARGET)
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    return usage;
  }

  VkMemoryPropertyFlags GetMemoryFlagsForUsage(
          DWORD                   Usage) {
    VkMemoryPropertyFlags memoryFlags = 0;

    if (Usage & D3DUSAGE_DYNAMIC) {
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else {
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    return memoryFlags;
  }

  uint32_t VertexCount(D3DPRIMITIVETYPE type, UINT count) {
    switch (type) {
      default:
      case D3DPT_TRIANGLELIST: return count * 3;
      case D3DPT_POINTLIST: return count;
      case D3DPT_LINELIST: return count * 2;
      case D3DPT_LINESTRIP: return count + 1;
      case D3DPT_TRIANGLESTRIP: return count + 2;
      case D3DPT_TRIANGLEFAN: return count + 2;
    }
  }

  DxvkInputAssemblyState InputAssemblyState(D3DPRIMITIVETYPE type) {
    switch (type) {
      default:
      case D3DPT_TRIANGLELIST:
        return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE, 0 };

      case D3DPT_POINTLIST:
        return { VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE, 0 };

      case D3DPT_LINELIST:
        return { VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE, 0 };

      case D3DPT_LINESTRIP:
        return { VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, VK_TRUE, 0 };

      case D3DPT_TRIANGLESTRIP:
        return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_TRUE, 0 };

      case D3DPT_TRIANGLEFAN:
        return { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, VK_TRUE, 0 };
    }
  }

  VkBlendFactor DecodeBlendFactor(D3DBLEND BlendFactor, bool IsAlpha) {
    switch (BlendFactor) {
      case D3DBLEND_ZERO:            return VK_BLEND_FACTOR_ZERO;
      default:
      case D3DBLEND_ONE:             return VK_BLEND_FACTOR_ONE;
      case D3DBLEND_SRCCOLOR:        return VK_BLEND_FACTOR_SRC_COLOR;
      case D3DBLEND_INVSRCCOLOR:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
      case D3DBLEND_SRCALPHA:        return VK_BLEND_FACTOR_SRC_ALPHA;
      case D3DBLEND_INVSRCALPHA:     return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      case D3DBLEND_DESTALPHA:       return VK_BLEND_FACTOR_DST_ALPHA;
      case D3DBLEND_INVDESTALPHA:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
      case D3DBLEND_DESTCOLOR:       return VK_BLEND_FACTOR_DST_COLOR;
      case D3DBLEND_INVDESTCOLOR:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
      case D3DBLEND_SRCALPHASAT:     return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
      case D3DBLEND_BOTHSRCALPHA:    return VK_BLEND_FACTOR_SRC1_ALPHA;
      case D3DBLEND_BOTHINVSRCALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
      case D3DBLEND_BLENDFACTOR:     return IsAlpha ? VK_BLEND_FACTOR_CONSTANT_ALPHA : VK_BLEND_FACTOR_CONSTANT_COLOR;
      case D3DBLEND_INVBLENDFACTOR:  return IsAlpha ? VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
      case D3DBLEND_SRCCOLOR2:       return VK_BLEND_FACTOR_SRC1_COLOR;
      case D3DBLEND_INVSRCCOLOR2:    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    }
  }

  VkBlendOp DecodeBlendOp(D3DBLENDOP BlendOp) {
    switch (BlendOp) {
      default:
      case D3DBLENDOP_ADD:          return VK_BLEND_OP_ADD;
      case D3DBLENDOP_SUBTRACT:     return VK_BLEND_OP_SUBTRACT;
      case D3DBLENDOP_REVSUBTRACT:  return VK_BLEND_OP_REVERSE_SUBTRACT;
      case D3DBLENDOP_MIN:          return VK_BLEND_OP_MIN;
      case D3DBLENDOP_MAX:          return VK_BLEND_OP_MAX;
    }
  }

  VkFilter DecodeFilter(D3DTEXTUREFILTERTYPE Filter) {
    switch (Filter) {
    case D3DTEXF_NONE:
    case D3DTEXF_POINT:
      return VK_FILTER_NEAREST;
    default:
      return VK_FILTER_LINEAR;
    }
  }

  VkSamplerMipmapMode DecodeMipFilter(D3DTEXTUREFILTERTYPE Filter) {
    switch (Filter) {
    case D3DTEXF_POINT:
    case D3DTEXF_NONE:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    default:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
  }

  bool IsAnisotropic(D3DTEXTUREFILTERTYPE Filter) {
    return Filter == D3DTEXF_ANISOTROPIC;
  }

  VkSamplerAddressMode DecodeAddressMode(D3DTEXTUREADDRESS Mode) {
    switch (Mode) {
      default:
      case D3DTADDRESS_WRAP:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
      case D3DTADDRESS_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      case D3DTADDRESS_CLAMP:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      case D3DTADDRESS_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
      case D3DTADDRESS_MIRRORONCE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
  }

  VkCompareOp DecodeCompareOp(D3DCMPFUNC Func) {
    switch (Func) {
      default:
      case D3DCMP_NEVER:        return VK_COMPARE_OP_NEVER;
      case D3DCMP_LESS:         return VK_COMPARE_OP_LESS;
      case D3DCMP_EQUAL:        return VK_COMPARE_OP_EQUAL;
      case D3DCMP_LESSEQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
      case D3DCMP_GREATER:      return VK_COMPARE_OP_GREATER;
      case D3DCMP_NOTEQUAL:     return VK_COMPARE_OP_NOT_EQUAL;
      case D3DCMP_GREATEREQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
      case D3DCMP_ALWAYS:       return VK_COMPARE_OP_ALWAYS;
    }
  }

  VkStencilOp DecodeStencilOp(D3DSTENCILOP Op) {
    switch (Op) {
      default:
      case D3DSTENCILOP_KEEP:    return VK_STENCIL_OP_KEEP;
      case D3DSTENCILOP_ZERO:    return VK_STENCIL_OP_ZERO;
      case D3DSTENCILOP_REPLACE: return VK_STENCIL_OP_REPLACE;
      case D3DSTENCILOP_INCRSAT: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
      case D3DSTENCILOP_DECRSAT: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
      case D3DSTENCILOP_INVERT:  return VK_STENCIL_OP_INVERT;
      case D3DSTENCILOP_INCR:    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
      case D3DSTENCILOP_DECR:    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
  }

  VkCullModeFlags DecodeCullMode(D3DCULL Mode) {
    switch (Mode) {
      case D3DCULL_NONE: return VK_CULL_MODE_NONE;
      case D3DCULL_CW:   return VK_CULL_MODE_FRONT_BIT;
      default:
      case D3DCULL_CCW:  return VK_CULL_MODE_BACK_BIT;
    }
  }

  VkPolygonMode DecodeFillMode(D3DFILLMODE Mode) {
    switch (Mode) {
      case D3DFILL_POINT:     return VK_POLYGON_MODE_POINT;
      case D3DFILL_WIREFRAME: return VK_POLYGON_MODE_LINE;
      default:
      case D3DFILL_SOLID:     return VK_POLYGON_MODE_FILL;
    }
  }

  VkIndexType DecodeIndexType(D3D9Format Format) {
    return Format == D3D9Format::INDEX16
                   ? VK_INDEX_TYPE_UINT16
                   : VK_INDEX_TYPE_UINT32;
  }

  uint32_t DecltypeSize(D3DDECLTYPE Type) {
    switch (Type) {
      case D3DDECLTYPE_FLOAT1:    return 1 * sizeof(float);
      case D3DDECLTYPE_FLOAT2:    return 2 * sizeof(float);
      case D3DDECLTYPE_FLOAT3:    return 3 * sizeof(float);
      case D3DDECLTYPE_FLOAT4:    return 4 * sizeof(float);
      case D3DDECLTYPE_D3DCOLOR:  return 1 * sizeof(DWORD);
      case D3DDECLTYPE_UBYTE4:    return 4 * sizeof(BYTE);
      case D3DDECLTYPE_SHORT2:    return 2 * sizeof(short);
      case D3DDECLTYPE_SHORT4:    return 4 * sizeof(short);
      case D3DDECLTYPE_UBYTE4N:   return 4 * sizeof(BYTE);
      case D3DDECLTYPE_SHORT2N:   return 2 * sizeof(short);
      case D3DDECLTYPE_SHORT4N:   return 4 * sizeof(short);
      case D3DDECLTYPE_USHORT2N:  return 2 * sizeof(short);
      case D3DDECLTYPE_USHORT4N:  return 4 * sizeof(short);
      case D3DDECLTYPE_UDEC3:     return 4;
      case D3DDECLTYPE_DEC3N:     return 4;
      case D3DDECLTYPE_FLOAT16_2: return 2 * 2;
      case D3DDECLTYPE_FLOAT16_4: return 4 * 2;
      default:                    return 0;
    }
  }

}