/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "event.h"
#include "dynloader.h"
#include "ext_vulkan.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *vk_result_string (VkResult rc)
{
  switch (rc)
  {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    default: return "VkResult: unknown";
  }
}

#define VK_LOAD(func) \
  vk->func = (PFN_##func) hc_dlsym (vk->lib, #func); \
  if (vk->func == NULL) return -1

int vk_init (void *hashcat_ctx)
{
  backend_ctx_t *backend_ctx = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx;

  VK_PTR *vk = (VK_PTR *) hccalloc (1, sizeof (VK_PTR));

  backend_ctx->vk = vk;

  vk->lib = hc_dlopen ("libvulkan.so.1");

  if (vk->lib == NULL) vk->lib = hc_dlopen ("libvulkan.so");

  if (vk->lib == NULL) return -1;

  vk->vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) hc_dlsym (vk->lib, "vkGetInstanceProcAddr");

  if (vk->vkGetInstanceProcAddr == NULL) return -1;

  vk->vkCreateInstance = (PFN_vkCreateInstance) vk->vkGetInstanceProcAddr (VK_NULL_HANDLE, "vkCreateInstance");
  vk->vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties) vk->vkGetInstanceProcAddr (VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");

  if (vk->vkCreateInstance == NULL) return -1;

  VK_LOAD (vkEnumeratePhysicalDevices);
  VK_LOAD (vkGetPhysicalDeviceProperties);
  VK_LOAD (vkGetPhysicalDeviceQueueFamilyProperties);
  VK_LOAD (vkGetPhysicalDeviceMemoryProperties);
  VK_LOAD (vkCreateDevice);
  VK_LOAD (vkGetDeviceQueue);
  VK_LOAD (vkDestroyInstance);
  VK_LOAD (vkDestroyDevice);

  VK_LOAD (vkCreateShaderModule);
  VK_LOAD (vkDestroyShaderModule);
  VK_LOAD (vkCreateDescriptorPool);
  VK_LOAD (vkDestroyDescriptorPool);
  VK_LOAD (vkCreateDescriptorSetLayout);
  VK_LOAD (vkDestroyDescriptorSetLayout);
  VK_LOAD (vkAllocateDescriptorSets);
  VK_LOAD (vkUpdateDescriptorSets);
  VK_LOAD (vkCreatePipelineLayout);
  VK_LOAD (vkDestroyPipelineLayout);
  VK_LOAD (vkCreateComputePipelines);
  VK_LOAD (vkDestroyPipeline);
  VK_LOAD (vkCreateCommandPool);
  VK_LOAD (vkDestroyCommandPool);
  VK_LOAD (vkAllocateCommandBuffers);
  VK_LOAD (vkFreeCommandBuffers);
  VK_LOAD (vkResetCommandPool);
  VK_LOAD (vkBeginCommandBuffer);
  VK_LOAD (vkEndCommandBuffer);
  VK_LOAD (vkResetCommandBuffer);
  VK_LOAD (vkCmdBindPipeline);
  VK_LOAD (vkCmdBindDescriptorSets);
  VK_LOAD (vkCmdPushConstants);
  VK_LOAD (vkCmdDispatch);
  VK_LOAD (vkCmdFillBuffer);
  VK_LOAD (vkCmdCopyBuffer);
  VK_LOAD (vkCmdPipelineBarrier);
  VK_LOAD (vkQueueSubmit);
  VK_LOAD (vkQueueWaitIdle);
  VK_LOAD (vkCreateFence);
  VK_LOAD (vkDestroyFence);
  VK_LOAD (vkResetFences);
  VK_LOAD (vkWaitForFences);
  VK_LOAD (vkCreateQueryPool);
  VK_LOAD (vkDestroyQueryPool);
  VK_LOAD (vkResetQueryPool);
  VK_LOAD (vkCmdResetQueryPool);
  VK_LOAD (vkCmdWriteTimestamp);
  VK_LOAD (vkGetQueryPoolResults);
  VK_LOAD (vkCreateBuffer);
  VK_LOAD (vkDestroyBuffer);
  VK_LOAD (vkAllocateMemory);
  VK_LOAD (vkFreeMemory);
  VK_LOAD (vkGetBufferMemoryRequirements);
  VK_LOAD (vkBindBufferMemory);
  VK_LOAD (vkMapMemory);
  VK_LOAD (vkUnmapMemory);

  return 0;
}

void vk_close (void *hashcat_ctx)
{
  backend_ctx_t *backend_ctx = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx;

  VK_PTR *vk = (VK_PTR *) backend_ctx->vk;

  if (vk)
  {
    if (vk->lib) hc_dlclose (vk->lib);

    hcfree (vk);

    backend_ctx->vk = NULL;
  }
}

int hc_vkEnumeratePhysicalDevices (void *hashcat_ctx, VkInstance instance, uint32_t *count, VkPhysicalDevice *devices)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  VkResult rc = vk->vkEnumeratePhysicalDevices (instance, count, devices);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkEnumeratePhysicalDevices(): %s", vk_result_string (rc));

    return -1;
  }

  return 0;
}

int hc_vkGetPhysicalDeviceProperties (void *hashcat_ctx, VkPhysicalDevice dev, VkPhysicalDeviceProperties *props)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  vk->vkGetPhysicalDeviceProperties (dev, props);

  return 0;
}

static int vk_find_memory_type (VK_PTR *vk, VkPhysicalDevice phys, uint32_t type_bits, VkMemoryPropertyFlags want)
{
  VkPhysicalDeviceMemoryProperties mp;

  vk->vkGetPhysicalDeviceMemoryProperties (phys, &mp);

  for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
  {
    if ((type_bits & (1u << i)) && ((mp.memoryTypes[i].propertyFlags & want) == want)) return i;
  }

  return -1;
}

static int vk_find_compute_family (VK_PTR *vk, VkPhysicalDevice phys)
{
  uint32_t nqf = 0;

  vk->vkGetPhysicalDeviceQueueFamilyProperties (phys, &nqf, NULL);

  if (nqf > 32) nqf = 32;

  VkQueueFamilyProperties qf[32];

  vk->vkGetPhysicalDeviceQueueFamilyProperties (phys, &nqf, qf);

  for (uint32_t i = 0; i < nqf; i++)
  {
    if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) return i;
  }

  return -1;
}

int hc_vkCreateDevice (void *hashcat_ctx, MAYBE_UNUSED VkInstance instance, VkPhysicalDevice dev, VkDevice *device, uint32_t *queue_family)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  int qfam = vk_find_compute_family (vk, dev);

  if (qfam == -1)
  {
    event_log_error (hashcat_ctx, "hc_vkCreateDevice(): no compute queue found");

    return -1;
  }

  float prio = 1.0f;

  VkDeviceQueueCreateInfo qci =
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = (uint32_t) qfam,
    .queueCount = 1,
    .pQueuePriorities = &prio
  };

  // hashcat kernels compiled by clspv need 64 bit integers (ulong scalars everywhere) and
  // declare the Int8 capability (void * pointers). RADV supports both, so request them
  // directly rather than trying to mirror the whole supported feature set

  VkPhysicalDeviceFeatures2 features2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = NULL };

  features2.features.shaderInt64 = VK_TRUE;
  features2.features.shaderInt16 = VK_TRUE;

  // some clspv kernels declare the VariablePointers capabilities

  VkPhysicalDeviceVariablePointerFeatures vp =
  {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTER_FEATURES,
    .pNext = NULL,
    .variablePointersStorageBuffer = VK_TRUE,
    .variablePointers = VK_TRUE
  };

  VkPhysicalDeviceShaderFloat16Int8Features f16i8 =
  {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,
    .pNext = &vp,
    .shaderFloat16 = VK_FALSE,
    .shaderInt8 = VK_TRUE
  };

  features2.pNext = &f16i8;

  const char *device_extensions[] = { "VK_KHR_shader_float16_int8" };

  VkDeviceCreateInfo dci =
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &features2,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &qci,
    .enabledExtensionCount = 1,
    .ppEnabledExtensionNames = device_extensions
  };

  VkResult rc = vk->vkCreateDevice (dev, &dci, NULL, device);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateDevice(): %s", vk_result_string (rc));

    return -1;
  }

  *queue_family = (uint32_t) qfam;

  return 0;
}

int hc_vkBufferAlloc (void *hashcat_ctx, VkDevice device, VkPhysicalDevice phys, hc_vk_buffer_t *buf, VkDeviceSize size)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  memset (buf, 0, sizeof (hc_vk_buffer_t));

  buf->device = device;
  buf->phys   = phys;
  buf->size   = size;

  if (size == 0) return 0;

  VkBufferCreateInfo bci =
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };

  VkResult rc = vk->vkCreateBuffer (device, &bci, NULL, &buf->buffer);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateBuffer(): %s", vk_result_string (rc));

    return -1;
  }

  VkMemoryRequirements mr;

  vk->vkGetBufferMemoryRequirements (device, buf->buffer, &mr);

  int memidx = vk_find_memory_type (vk, phys, mr.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (memidx == -1)
  {
    event_log_error (hashcat_ctx, "hc_vkBufferAlloc(): no host visible memory type found");

    return -1;
  }

  VkMemoryAllocateInfo mai =
  {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mr.size,
    .memoryTypeIndex = (uint32_t) memidx
  };

  rc = vk->vkAllocateMemory (device, &mai, NULL, &buf->mem);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkAllocateMemory(): %s", vk_result_string (rc));

    return -1;
  }

  rc = vk->vkBindBufferMemory (device, buf->buffer, buf->mem, 0);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkBindBufferMemory(): %s", vk_result_string (rc));

    return -1;
  }

  rc = vk->vkMapMemory (device, buf->mem, 0, VK_WHOLE_SIZE, 0, &buf->host);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkMapMemory(): %s", vk_result_string (rc));

    return -1;
  }

  return 0;
}

// forward declaration: lazy GPU transfer context helpers live at the bottom,
// but BufferFree must flush a pending transfer before releasing its memory

static int vk_transfer_wait (void *hashcat_ctx, hc_vk_buffer_t *buf, const char *what);

int hc_vkBufferFree (void *hashcat_ctx, hc_vk_buffer_t *buf)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  // never free memory referenced by an in-flight transfer

  if (buf->copy_pending)
  {
    vk_transfer_wait (hashcat_ctx, buf, "free");
  }

  // unmap is implicit on FreeMemory

  if (buf->mem != VK_NULL_HANDLE) vk->vkFreeMemory (buf->device, buf->mem, NULL);
  if (buf->buffer != VK_NULL_HANDLE) vk->vkDestroyBuffer (buf->device, buf->buffer, NULL);

  // GPU transfer context, lazily created by vk_copy_ctx_ensure()

  if (buf->copy_ready)
  {
    if (buf->copy_pool != VK_NULL_HANDLE) vk->vkDestroyCommandPool (buf->device, buf->copy_pool, NULL);
    if (buf->copy_fence != VK_NULL_HANDLE) vk->vkDestroyFence (buf->device, buf->copy_fence, NULL);
  }

  memset (buf, 0, sizeof (hc_vk_buffer_t));

  return 0;
}

// ---------------------------------------------------------------------------
// SPIR-V binary parsing of the NonSemantic.ClspvReflection section.
//
// clspv eliminates unused kernel arguments, so descriptor bindings are
// compacted and do not match hashcat's argument indices. The reflection
// metadata maps original argument ordinals to bindings, which is what makes
// a clSetKernelArg-style host API possible.
// ---------------------------------------------------------------------------

#define CLSPV_REFL_KERNEL                 1
#define CLSPV_REFL_ARGUMENT_INFO          2
#define CLSPV_REFL_ARGUMENT_STORAGE_BUF   3
#define CLSPV_REFL_ARGUMENT_UNIFORM       4
#define CLSPV_REFL_ARGUMENT_LOCAL         5
#define CLSPV_REFL_ARGUMENT_POD_STORAGE   6
#define CLSPV_REFL_ARGUMENT_POD_PUSH      7
#define CLSPV_REFL_ARGUMENT_POD_UNIFORM   8

typedef struct
{
  // opcode 11: ext import
  uint32_t refl_set;

  // opcode 15: entry points
  uint32_t entry_id[64];
  char     entry_name[64][128];
  uint32_t entry_cnt;

  // opcode 43: constants (id -> value)
  // large modules (hand-written natives include big constant tables) can
  // exceed 8k constants; injected reflection constants sit at the end and
  // must not be dropped

  uint32_t const_id[32768];
  uint32_t const_val[32768];
  uint32_t const_cnt;

  // kernel extinst result id -> entry point function id
  uint32_t kern_result[64];
  uint32_t kern_fn[64];
  uint32_t kern_cnt;

  // storage buffer and pod-pushconstant arguments: kernel result id, ordinal + extras

  uint32_t arg_kernel[2048];
  int32_t  arg_ordinal[2048];
  int32_t  arg_binding[2048];
  int32_t  arg_extra1[2048];
  int32_t  arg_extra2[2048];
  uint32_t arg_kind[2048];
  uint32_t arg_cnt;

} vk_refl_parse_t;

static void vk_refl_parse (const uint32_t *code, size_t code_words, vk_refl_parse_t *p)
{
  memset (p, 0, sizeof (vk_refl_parse_t));

  if (code_words < 5) return;

  size_t pos = 5; // skip header

  while (pos < code_words)
  {
    uint32_t insn = code[pos];

    uint32_t op = insn & 0xffff;
    uint32_t wc = insn >> 16;

    if (wc == 0) break;

    const uint32_t *ops = &code[pos + 1];
    uint32_t nops = wc - 1;

    switch (op)
    {
      case 11: // OpExtInstImport
        {
          // last word(s) are literal string; name is in first non-id operand slot 1

          const char *name = (const char *) &ops[1];

          const size_t want = strlen ("NonSemantic.ClspvReflection");
          const size_t have = strnlen (name, (nops - 1) * sizeof (uint32_t));

          if (have >= want && strncmp (name, "NonSemantic.ClspvReflection", want) == 0)
          {
            p->refl_set = ops[0];
          }
        }
        break;

      case 15: // OpEntryPoint
        if (p->entry_cnt < 64 && nops >= 3)
        {
          p->entry_id[p->entry_cnt] = ops[1];

          const char *name = (const char *) &ops[2];
          size_t maxch = (nops - 2) * sizeof (uint32_t);
          size_t len = strnlen (name, maxch);

          if (len >= 128) len = 127;

          memcpy (p->entry_name[p->entry_cnt], name, len);
          p->entry_name[p->entry_cnt][len] = 0;

          p->entry_cnt++;
        }
        break;

      case 43: // OpConstant
        if (p->const_cnt < 32768 && nops >= 3)
        {
          p->const_id[p->const_cnt] = ops[1];
          p->const_val[p->const_cnt] = ops[2];
          p->const_cnt++;
        }
        break;

      case 12: // OpExtInst
        if (nops >= 4 && ops[2] == p->refl_set)
        {
          uint32_t result = ops[1];
          uint32_t inst = ops[3];
          const uint32_t *iops = &ops[4];
          uint32_t iops_n = nops - 4;

          if (inst == CLSPV_REFL_KERNEL && iops_n >= 1 && p->kern_cnt < 64)
          {
            p->kern_result[p->kern_cnt] = result;
            p->kern_fn[p->kern_cnt] = iops[0];
            p->kern_cnt++;
          }
          else if ((inst == CLSPV_REFL_ARGUMENT_STORAGE_BUF || inst == CLSPV_REFL_ARGUMENT_LOCAL
                    || inst == CLSPV_REFL_ARGUMENT_POD_STORAGE || inst == CLSPV_REFL_ARGUMENT_POD_PUSH
                    || inst == CLSPV_REFL_ARGUMENT_POD_UNIFORM || inst == CLSPV_REFL_ARGUMENT_UNIFORM)
                   && iops_n >= 4 && p->arg_cnt < 2048)
          {
            p->arg_kernel[p->arg_cnt] = iops[0];
            p->arg_kind[p->arg_cnt] = inst;

            // numeric operands arrive as references to OpConstant ids

            p->arg_ordinal[p->arg_cnt] = -1;
            p->arg_binding[p->arg_cnt] = -1;
            p->arg_extra1[p->arg_cnt] = -1;
            p->arg_extra2[p->arg_cnt] = -1;

            for (uint32_t c = 0; c < p->const_cnt; c++)
            {
              if (p->const_id[c] == iops[1]) { p->arg_ordinal[p->arg_cnt] = p->const_val[c]; break; }
            }

            for (uint32_t c = 0; c < p->const_cnt; c++)
            {
              if (p->const_id[c] == iops[2]) { p->arg_extra1[p->arg_cnt] = p->const_val[c]; break; }
            }

            for (uint32_t c = 0; c < p->const_cnt; c++)
            {
              if (p->const_id[c] == iops[3]) { p->arg_binding[p->arg_cnt] = p->const_val[c]; break; }
            }

            for (uint32_t c = 0; c < p->const_cnt; c++)
            {
              if (p->const_id[c] == iops[3]) { p->arg_extra2[p->arg_cnt] = p->const_val[c]; break; }
              if (p->const_id[c] == iops[2]) { p->arg_extra2[p->arg_cnt] = p->const_val[c]; }
            }

            p->arg_cnt++;
          }
        }
        break;
    }

    pos += wc;
  }
}

static int32_t vk_const_value (vk_refl_parse_t *p, uint32_t id)
{
  for (uint32_t c = 0; c < p->const_cnt; c++)
  {
    if (p->const_id[c] == id) return p->const_val[c];
  }

  return -1;
}

int hc_vkProgramCreate (void *hashcat_ctx, VkDevice device, const char *spirv, size_t spirv_size, vk_program *program, hc_vk_refl_t **refl)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if ((spirv_size % 4) != 0)
  {
    event_log_error (hashcat_ctx, "hc_vkProgramCreate(): SPIR-V size is not a multiple of 4");

    return -1;
  }

  VkShaderModuleCreateInfo smci =
  {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = spirv_size,
    .pCode = (const uint32_t *) spirv
  };

  VkShaderModule sm = VK_NULL_HANDLE;

  VkResult rc = vk->vkCreateShaderModule (device, &smci, NULL, &sm);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateShaderModule(): %s", vk_result_string (rc));

    return -1;
  }

  *program = (vk_program) sm;

  *refl = NULL;

  // extract per-entrypoint ordinal -> binding maps

  vk_refl_parse_t P;

  vk_refl_parse ((const uint32_t *) spirv, spirv_size / 4, &P);

  hc_vk_refl_t *r = (hc_vk_refl_t *) hccalloc (1, sizeof (hc_vk_refl_t));

  r->count = P.entry_cnt;

  if (r->count > 0)
  {
    r->entries = (hc_vk_refl_entry_t *) hccalloc (r->count, sizeof (hc_vk_refl_entry_t));

    for (uint32_t e = 0; e < r->count; e++)
    {
      strncpy (r->entries[e].name, P.entry_name[e], 127);

      for (uint32_t b = 0; b < HC_VK_MAX_BINDINGS; b++)
      {
        r->entries[e].ord_to_binding[b] = -1;
        r->entries[e].pod_offset[b] = -1;
        r->entries[e].pod_size[b] = -1;
      }
    }

    for (uint32_t k = 0; k < P.kern_cnt; k++)
    {
      uint32_t fn = P.kern_fn[k];
      uint32_t kr = P.kern_result[k];

      for (uint32_t e = 0; e < r->count; e++)
      {
        if (P.entry_id[e] != fn) continue;

        for (uint32_t a = 0; a < P.arg_cnt; a++)
        {
          if (P.arg_kernel[a] != kr) continue;

          if (P.arg_kind[a] == CLSPV_REFL_ARGUMENT_STORAGE_BUF)
          {
            if (P.arg_ordinal[a] >= 0 && P.arg_ordinal[a] < HC_VK_MAX_BINDINGS)
            {
              r->entries[e].ord_to_binding[P.arg_ordinal[a]] = P.arg_binding[a];
            }
          }
          else if (P.arg_kind[a] == CLSPV_REFL_ARGUMENT_POD_PUSH)
          {
            if (P.arg_ordinal[a] >= 0 && P.arg_ordinal[a] < HC_VK_MAX_BINDINGS)
            {
              r->entries[e].pod_offset[P.arg_ordinal[a]] = P.arg_extra1[a];
              r->entries[e].pod_size[P.arg_ordinal[a]] = P.arg_extra2[a];
            }
          }
        }
      }
    }
  }

  *refl = r;

  return 0;
}

void hc_vkReflFree (void *hashcat_ctx, hc_vk_refl_t *r)
{
  if (r == NULL) return;

  hcfree (r->entries);

  hcfree (r);
}

int hc_vkProgramDestroy (void *hashcat_ctx, VkDevice device, vk_program program)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  vk->vkDestroyShaderModule (device, (VkShaderModule) program, NULL);

  return 0;
}

int hc_vkKernelInit (void *hashcat_ctx, VkDevice device, VkPhysicalDevice phys, vk_kernel *kernel)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  vk_kernel k = (vk_kernel) hccalloc (1, sizeof (hc_vk_kernel_t));

  if (k == NULL) return -1;

  k->device = device;
  k->phys   = phys;

  *kernel = k;

  for (uint32_t i = 0; i < HC_VK_MAX_BINDINGS; i++)
  {
    k->ord_to_binding[i] = -1;
    k->pod_offset[i] = -1;
    k->pod_size[i] = -1;
  }

  VkDescriptorSetLayoutBinding lb[HC_VK_MAX_BINDINGS];

  memset (lb, 0, sizeof (lb));

  for (uint32_t i = 0; i < HC_VK_MAX_BINDINGS; i++)
  {
    lb[i].binding = i;
    lb[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lb[i].descriptorCount = 1;
    lb[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  }

  VkDescriptorSetLayoutCreateInfo dlci =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = HC_VK_MAX_BINDINGS,
    .pBindings = lb
  };

  VkResult rc = vk->vkCreateDescriptorSetLayout (device, &dlci, NULL, &k->dslayout);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateDescriptorSetLayout(): %s", vk_result_string (rc));

    return -1;
  }

  VkDescriptorPoolSize ps =
  {
    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = HC_VK_MAX_BINDINGS * HC_VK_SLOTS
  };

  VkDescriptorPoolCreateInfo pci =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = HC_VK_SLOTS,
    .poolSizeCount = 1,
    .pPoolSizes = &ps
  };

  rc = vk->vkCreateDescriptorPool (device, &pci, NULL, &k->pool);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateDescriptorPool(): %s", vk_result_string (rc));

    return -1;
  }

  VkDescriptorSetLayout dlayouts[HC_VK_SLOTS];

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    dlayouts[i] = k->dslayout;
  }

  VkDescriptorSet dsets[HC_VK_SLOTS];

  VkDescriptorSetAllocateInfo dsai =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = k->pool,
    .descriptorSetCount = HC_VK_SLOTS,
    .pSetLayouts = dlayouts
  };

  rc = vk->vkAllocateDescriptorSets (device, &dsai, dsets);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkAllocateDescriptorSets(): %s", vk_result_string (rc));

    return -1;
  }

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    k->slots[i].dset       = dsets[i];
    k->slots[i].dset_valid = 0;
    k->slots[i].pod_ready  = 0;
  }

  VkPipelineLayoutCreateInfo plci =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &k->dslayout
  };

  // clspv generated kernels always declare a small push constant block (normalization
  // constants), so the pipeline layout has to offer a matching range even though hashcat
  // itself never pushes data

  VkPushConstantRange pcr = { .stageFlags = 0, .offset = 0, .size = 0 };

  {
    VkPhysicalDeviceProperties props;

    vk->vkGetPhysicalDeviceProperties (phys, &props);

    if (props.limits.maxPushConstantsSize > 0)
    {
      pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      pcr.size = props.limits.maxPushConstantsSize;

      plci.pushConstantRangeCount = 1;
      plci.pPushConstantRanges = &pcr;
    }
  }

  rc = vk->vkCreatePipelineLayout (device, &plci, NULL, &k->layout);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreatePipelineLayout(): %s", vk_result_string (rc));

    return -1;
  }

  int qfam = vk_find_compute_family (vk, phys);

  if (qfam == -1) qfam = 0;

  VkCommandPoolCreateInfo cpolci =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = (uint32_t) qfam
  };

  rc = vk->vkCreateCommandPool (device, &cpolci, NULL, &k->cmdpool);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateCommandPool(): %s", vk_result_string (rc));

    return -1;
  }

  VkCommandBufferAllocateInfo cbai =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = k->cmdpool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = HC_VK_SLOTS
  };

  VkCommandBuffer cmds[HC_VK_SLOTS];

  rc = vk->vkAllocateCommandBuffers (device, &cbai, cmds);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkAllocateCommandBuffers(): %s", vk_result_string (rc));

    return -1;
  }

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    k->slots[i].cmdbuf     = cmds[i];
    k->slots[i].fence      = VK_NULL_HANDLE;
    k->slots[i].submitted  = 0;
    k->slots[i].ts_pending = 0;
  }

  k->slot_next = 0;

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    rc = vk->vkCreateFence (device, &(VkFenceCreateInfo) { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO }, NULL, &k->slots[i].fence);

    if (rc != VK_SUCCESS)
    {
      event_log_error (hashcat_ctx, "vkCreateFence(): %s", vk_result_string (rc));

      return -1;
    }
  }

  // timestamp query pool for kernel timing

  uint32_t nqf = 0;

  vk->vkGetPhysicalDeviceQueueFamilyProperties (phys, &nqf, NULL);

  if (nqf > 32) nqf = 32;

  VkQueueFamilyProperties qf[32];

  vk->vkGetPhysicalDeviceQueueFamilyProperties (phys, &nqf, qf);

  VkQueryPoolCreateInfo qpci =
  {
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .queryType = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = 2 * HC_VK_SLOTS
  };

  rc = vk->vkCreateQueryPool (device, &qpci, NULL, &k->querypool);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateQueryPool(): %s", vk_result_string (rc));

    return -1;
  }

  VkPhysicalDeviceProperties props;

  vk->vkGetPhysicalDeviceProperties (phys, &props);

  if (qf[qfam].timestampValidBits > 0 && props.limits.timestampPeriod > 0.0f)
  {
    k->timestamps_supported = 1;
    k->timestamp_period = (double) props.limits.timestampPeriod;
  }

  return 0;
}

int hc_vkKernelTerm (void *hashcat_ctx, vk_kernel k)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if (k == NULL) return 0;

  // never free objects that may still be referenced by in-flight work

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    if (k->slots[i].submitted)
    {
      vk->vkWaitForFences (k->device, 1, &k->slots[i].fence, VK_TRUE, 10ull * 1000ull * 1000ull * 1000ull);
    }
  }

  if (k->pipeline != VK_NULL_HANDLE) vk->vkDestroyPipeline (k->device, k->pipeline, NULL);
  if (k->entrypoint) hcfree (k->entrypoint);
  if (k->querypool != VK_NULL_HANDLE) vk->vkDestroyQueryPool (k->device, k->querypool, NULL);

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    if (k->slots[i].fence != VK_NULL_HANDLE) vk->vkDestroyFence (k->device, k->slots[i].fence, NULL);

    if (k->slots[i].pod_ready != 0) hc_vkBufferFree (hashcat_ctx, &k->slots[i].pod_buf);
  }

  if (k->cmdpool != VK_NULL_HANDLE) vk->vkDestroyCommandPool (k->device, k->cmdpool, NULL);
  if (k->layout != VK_NULL_HANDLE) vk->vkDestroyPipelineLayout (k->device, k->layout, NULL);
  if (k->pool != VK_NULL_HANDLE) vk->vkDestroyDescriptorPool (k->device, k->pool, NULL);
  if (k->dslayout != VK_NULL_HANDLE) vk->vkDestroyDescriptorSetLayout (k->device, k->dslayout, NULL);

  hcfree (k);

  return 0;
}

// compile (or recompile with a new local size) the entry point of a program into a pipeline

int hc_vkKernelCompile (void *hashcat_ctx, vk_kernel k, vk_program program, hc_vk_refl_t *refl, const char *entrypoint, uint32_t lszx, uint32_t lszy)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if (k->pipeline != VK_NULL_HANDLE)
  {
    if (k->local_size_x == lszx && k->local_size_y == lszy && strcmp (k->entrypoint, entrypoint) == 0) return 0;

    // the pipeline can still be referenced by an in-flight command buffer

    for (int i = 0; i < HC_VK_SLOTS; i++)
    {
      if (k->slots[i].submitted)
      {
        vk->vkWaitForFences (k->device, 1, &k->slots[i].fence, VK_TRUE, 10ull * 1000ull * 1000ull * 1000ull);
      }
    }

    vk->vkDestroyPipeline (k->device, k->pipeline, NULL);

    k->pipeline = VK_NULL_HANDLE;
  }

  // look up the ordinal -> binding map for this entry point

  int found = -1;

  if (refl != NULL)
  {
    for (uint32_t e = 0; e < refl->count; e++)
    {
      if (strcmp (refl->entries[e].name, entrypoint) == 0) { found = e; break; }
    }
  }

  if (found == -1)
  {
    event_log_error (hashcat_ctx, "hc_vkKernelCompile(): entry point '%s' not found in module", entrypoint);

    return -1;
  }

  memcpy (k->ord_to_binding, refl->entries[found].ord_to_binding, sizeof (k->ord_to_binding));
  memcpy (k->pod_offset, refl->entries[found].pod_offset, sizeof (k->pod_offset));
  memcpy (k->pod_size, refl->entries[found].pod_size, sizeof (k->pod_size));

  uint32_t lsize[3] = { lszx ? lszx : 1, lszy ? lszy : 1, 1 };

  VkSpecializationMapEntry sentries[3] =
  {
    { 0, 0 * sizeof (uint32_t), sizeof (uint32_t) },
    { 1, 1 * sizeof (uint32_t), sizeof (uint32_t) },
    { 2, 2 * sizeof (uint32_t), sizeof (uint32_t) }
  };

  VkSpecializationInfo sinfo =
  {
    .mapEntryCount = 3,
    .pMapEntries = sentries,
    .dataSize = sizeof (lsize),
    .pData = lsize
  };

  VkPipelineShaderStageCreateInfo stage =
  {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    .module = (VkShaderModule) program,
    .pName = entrypoint,
    .pSpecializationInfo = &sinfo
  };

  VkComputePipelineCreateInfo cpi =
  {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = stage,
    .layout = k->layout
  };

  if (getenv ("HASHCAT_VK_DEBUG"))
  {
    fprintf (stderr, "[vk] vkCreateComputePipelines %s (%ux%u)\n", entrypoint ? entrypoint : "?",
             lsize[0], lsize[1]);
    fflush (stderr);
  }

  VkResult rc = vk->vkCreateComputePipelines (k->device, VK_NULL_HANDLE, 1, &cpi, NULL, &k->pipeline);

  if (getenv ("HASHCAT_VK_DEBUG"))
  {
    fprintf (stderr, "[vk] pipeline created %s\n", entrypoint ? entrypoint : "?");
    fflush (stderr);
  }

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkCreateComputePipelines(%s): %s", entrypoint ? entrypoint : "(null)", vk_result_string (rc));

    return -1;
  }

  // dup before freeing: callers may legitimately pass k->entrypoint itself

  char *entrypoint_dup = hcstrdup (entrypoint);

  if (k->entrypoint) hcfree (k->entrypoint);

  k->entrypoint = entrypoint_dup;
  k->local_size_x = lsize[0];
  k->local_size_y = lsize[1];
  k->local_size_z = lsize[2];

  k->dirty = 1;

  // the ordinal -> binding map changed: every slot's descriptor set is stale

  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    k->slots[i].dset_valid = 0;
  }

  return 0;
}

int hc_vkSetKernelArg (void *hashcat_ctx, vk_kernel k, uint32_t index, hc_vk_buffer_t *value)
{
  if (index >= HC_VK_MAX_BINDINGS)
  {
    event_log_error (hashcat_ctx, "hc_vkSetKernelArg(): argument index %u out of range", index);

    return -1;
  }

  // eliminated arguments have no binding; setting them is a no-op

  if (k->ord_to_binding[index] == -1) return 0;

  if (k->bindings[index] != value->buffer)
  {
    k->bindings[index] = value->buffer;
    k->dirty = 1;

    // every slot's descriptor set holds its own copy of the bindings

    for (int i = 0; i < HC_VK_SLOTS; i++)
    {
      k->slots[i].dset_valid = 0;
    }
  }

  return 0;
}

// gx/gy are total work-item counts; converted to group counts internally
//   (extra tail threads self-limit via the GID_CNT guard every kernel starts with)
int hc_vkQueueIdle (void *hashcat_ctx, VkQueue queue)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  VkResult rc = vk->vkQueueWaitIdle (queue);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkQueueWaitIdle(): %s", vk_result_string (rc));

    return -1;
  }

  return 0;
}

// read this slot's finished timestamp pair into exec_ms (deferred timing)

static void vk_slot_read_timestamps (VK_PTR *vk, vk_kernel k, hc_vk_slot_t *s, double *exec_ms)
{
  const uint32_t q0 = (uint32_t) (s - k->slots) * 2;

  uint64_t ts[2] = { 0, 0 };

  VkResult rc = vk->vkGetQueryPoolResults (k->device, k->querypool, q0, 2, sizeof (ts), ts, sizeof (uint64_t), VK_QUERY_RESULT_64_BIT);

  if (rc == VK_SUCCESS && ts[1] > ts[0] && exec_ms)
  {
    *exec_ms = (double) (ts[1] - ts[0]) * k->timestamp_period / 1e6;
  }

  if (getenv ("HASHCAT_VK_DEBUG"))
  {
    fprintf (stderr, "[vk] ts %s slot=%d rc=%s ts=[%llu %llu] period=%f exec_ms=%f\n",
             k->entrypoint ? k->entrypoint : "?", (int) (q0 / 2), vk_result_string (rc),
             (unsigned long long) ts[0], (unsigned long long) ts[1],
             k->timestamp_period, exec_ms ? *exec_ms : 0.0);
    fflush (stderr);
  }
}

// wait for all in-flight slots of a kernel. no longer used by the dispatch
// path (descriptor sets are slot-owned now), kept for callers that need to
// drain a kernel completely.

MAYBE_UNUSED
static int vk_kernel_wait_slots (VK_PTR *vk, vk_kernel k, void *hashcat_ctx)
{
  for (int i = 0; i < HC_VK_SLOTS; i++)
  {
    if (k->slots[i].submitted == 0) continue;

    VkResult rc = vk->vkWaitForFences (k->device, 1, &k->slots[i].fence, VK_TRUE, 10ull * 1000ull * 1000ull * 1000ull);

    if (rc != VK_SUCCESS)
    {
      event_log_error (hashcat_ctx, "vkWaitForFences(%s): %s", k->entrypoint ? k->entrypoint : "?", vk_result_string (rc));

      return -1;
    }

    k->slots[i].submitted = 0;
  }

  return 0;
}

int hc_vkDispatch (void *hashcat_ctx, VkQueue queue, vk_kernel k, uint64_t gx, uint64_t gy, hc_vk_buffer_t *pod_buffer, double *exec_ms)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if (k->pipeline == VK_NULL_HANDLE)
  {
    event_log_error (hashcat_ctx, "hc_vkDispatch(): kernel has no pipeline");

    return -1;
  }

  if (gx == 0 || gy == 0)
  {
    event_log_error (hashcat_ctx, "hc_vkDispatch(): zero work groups");

    return -1;
  }

  if (exec_ms) *exec_ms = 0;

  const int dbg = (getenv ("HASHCAT_VK_DEBUG") != NULL);

  if (getenv ("HASHCAT_VK_DUMP") != NULL && k->entrypoint != NULL && strstr (k->entrypoint, "markov") != NULL)
  {
    fprintf (stderr, "[vkpod] %s:", k->entrypoint);

    if (pod_buffer != NULL && pod_buffer->host != NULL)
    {
      const uint64_t *p = (const uint64_t *) pod_buffer->host;

      fprintf (stderr, " off=%llu len32=%08x %08x %08x %08x gid=%llu",
               (unsigned long long) p[0],
               ((const unsigned int *) pod_buffer->host)[2], ((const unsigned int *) pod_buffer->host)[3],
               ((const unsigned int *) pod_buffer->host)[4], ((const unsigned int *) pod_buffer->host)[5],
               (unsigned long long) p[4]);
    }

    fprintf (stderr, " gx=%llu\n", (unsigned long long) gx);
    fflush (stderr);
  }

  if (dbg)
  {
    fprintf (stderr, "[vk] dispatch %s gx=%llu gy=%llu\n", k->entrypoint ? k->entrypoint : "?",
             (unsigned long long) gx, (unsigned long long) gy);
    fflush (stderr);
  }

  // HASHCAT_VK_SYNC=1 gives the old fully-blocking behavior for every dispatch.
  // Without it, blocking applies everywhere except the attack loop's async window
  // (vk->async_enabled == 1). Timed callers inside the window receive the deferred
  // measurement of the previous same-kernel run instead of blocking (see below).

  const int blocking = (getenv ("HASHCAT_VK_SYNC") != NULL) || (vk->async_enabled == 0);

  hc_vk_slot_t *slot = &k->slots[k->slot_next];

  if (blocking)
  {
    vk->vkQueueWaitIdle (queue);
  }
  else
  {
    // pipelined: only wait for the fence of the slot we are about to reuse,
    // which the GPU finished working through some kernels ago

    if (slot->submitted)
    {
      VkResult rc = vk->vkWaitForFences (k->device, 1, &slot->fence, VK_TRUE, 10ull * 1000ull * 1000ull * 1000ull);

      if (rc != VK_SUCCESS)
      {
        event_log_error (hashcat_ctx, "hc_vkDispatch(%s): %s after 10s (slot fence)",
                         k->entrypoint ? k->entrypoint : "?", vk_result_string (rc));

        if (rc == VK_TIMEOUT)
        {
          vk->vkQueueWaitIdle (queue);
        }

        return -1;
      }

      slot->submitted = 0;
    }

    // the work this slot previously carried finished: harvest its timestamps as
    // the (deferred) exec time for this kernel. first dispatches have no history,
    // so callers see 0 until one full slot rotation has gone by.

    if (slot->ts_pending)
    {
      vk_slot_read_timestamps (vk, k, slot, exec_ms);

      slot->ts_pending = 0;
    }
  }

  // snapshot the caller's scalar/POD argument buffer into this slot's private
  // copy. the buffer kernels read scalar arguments from (vk_d_kernel_param)
  // is refilled by the very next launch, so an in-flight dispatch would race
  // its own update: this is what made slow-hash kernel runs read half-written
  // loop_pos/loop_cnt values. every ordinal bound to that buffer is pointed
  // at the slot copy below.

  int pod_present = 0;

  if ((pod_buffer != NULL) && (pod_buffer->buffer != VK_NULL_HANDLE) && (pod_buffer->host != NULL))
  {
    for (uint32_t i = 0; i < HC_VK_MAX_BINDINGS; i++)
    {
      if (k->bindings[i] == pod_buffer->buffer) { pod_present = 1; break; }
    }
  }

  if (pod_present == 1)
  {
    if (slot->pod_ready == 0)
    {
      if (hc_vkBufferAlloc (hashcat_ctx, k->device, k->phys, &slot->pod_buf, pod_buffer->size) != 0) return -1;

      slot->pod_ready = 1;
    }

    memcpy (slot->pod_buf.host, pod_buffer->host, pod_buffer->size);

    // this slot's set must now reference the snapshot instead of the shared buffer

    slot->dset_valid = 0;
  }

  // rewrite this slot's descriptor set if its bindings changed (or the pod
  // redirection above requires it). the set can only be referenced by this
  // slot's own previous submission, whose fence we just waited on.

  if ((k->dirty != 0) && (getenv ("HASHCAT_VK_DEBUG") != NULL) && (k->entrypoint != NULL) && (strstr (k->entrypoint, "aux2") != NULL))
  {
    fprintf (stderr, "[vkdbg] aux2 descriptors:");
    for (uint32_t i = 0; i < HC_VK_MAX_BINDINGS; i++)
    {
      if (k->bindings[i] == VK_NULL_HANDLE) continue;
      fprintf (stderr, " ord%u->bind%d", i, k->ord_to_binding[i]);
    }
    fprintf (stderr, "\n");
  }

  if (slot->dset_valid == 0)
  {
    VkDescriptorBufferInfo binfo[HC_VK_MAX_BINDINGS];
    VkWriteDescriptorSet wr[HC_VK_MAX_BINDINGS];

    uint32_t n = 0;

    for (uint32_t i = 0; i < HC_VK_MAX_BINDINGS; i++)
    {
      if (k->bindings[i] == VK_NULL_HANDLE) continue;

      const int binding = k->ord_to_binding[i];

      if (binding < 0 || binding >= HC_VK_MAX_BINDINGS) continue;

      VkBuffer buf = k->bindings[i];

      if ((pod_present == 1) && (buf == pod_buffer->buffer))
      {
        buf = slot->pod_buf.buffer;
      }

      binfo[n].buffer = buf;
      binfo[n].offset = 0;
      binfo[n].range = VK_WHOLE_SIZE;

      wr[n].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      wr[n].pNext = NULL;
      wr[n].dstSet = slot->dset;
      wr[n].dstBinding = (uint32_t) binding;
      wr[n].dstArrayElement = 0;
      wr[n].descriptorCount = 1;
      wr[n].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      wr[n].pImageInfo = NULL;
      wr[n].pBufferInfo = &binfo[n];
      wr[n].pTexelBufferView = NULL;

      n++;
    }

    vk->vkUpdateDescriptorSets (k->device, n, wr, 0, NULL);

    slot->dset_valid = 1;
  }

  // record

  VkResult rc = vk->vkResetCommandBuffer (slot->cmdbuf, 0);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkResetCommandBuffer(): %s", vk_result_string (rc));

    return -1;
  }

  VkCommandBufferBeginInfo bi =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  rc = vk->vkBeginCommandBuffer (slot->cmdbuf, &bi);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkBeginCommandBuffer(): %s", vk_result_string (rc));

    return -1;
  }

  if (k->timestamps_supported)
  {
    const uint32_t q0 = (uint32_t) (slot - k->slots) * 2;

    vk->vkCmdResetQueryPool (slot->cmdbuf, k->querypool, q0, 2);
    vk->vkCmdWriteTimestamp (slot->cmdbuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, k->querypool, q0);
  }

  vk->vkCmdBindPipeline (slot->cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, k->pipeline);
  vk->vkCmdBindDescriptorSets (slot->cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, k->layout, 0, 1, &slot->dset, 0, NULL);

  // scalar arguments live in the push constant interface; copy their values
  // from the caller's staging buffer into the push constant block

  if ((pod_buffer != NULL) && (pod_buffer->host != NULL))
  {
    for (uint32_t i = 0; i < HC_VK_MAX_BINDINGS; i++)
    {
      if (k->pod_offset[i] < 0 || k->pod_size[i] <= 0) continue;

      vk->vkCmdPushConstants (slot->cmdbuf, k->layout, VK_SHADER_STAGE_COMPUTE_BIT,
                              (uint32_t) k->pod_offset[i], (uint32_t) k->pod_size[i],
                              (const char *) pod_buffer->host + k->pod_offset[i]);
    }
  }

  // gx/gy are work-item counts (same meaning as OpenCL's global_work_size);
  // vkCmdDispatch() takes group counts

  const uint64_t groups_x = (gx + k->local_size_x - 1) / k->local_size_x;
  const uint64_t groups_y = (gy + k->local_size_y - 1) / k->local_size_y;

  vk->vkCmdDispatch (slot->cmdbuf, (uint32_t) groups_x, (uint32_t) groups_y, 1);

  if (k->timestamps_supported)
  {
    vk->vkCmdWriteTimestamp (slot->cmdbuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, k->querypool,
                             (uint32_t) (slot - k->slots) * 2 + 1);
  }

  rc = vk->vkEndCommandBuffer (slot->cmdbuf);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkEndCommandBuffer(): %s", vk_result_string (rc));

    return -1;
  }

  // submit

  rc = vk->vkResetFences (k->device, 1, &slot->fence);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkResetFences(): %s", vk_result_string (rc));

    return -1;
  }

  VkSubmitInfo si =
  {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &slot->cmdbuf
  };

  rc = vk->vkQueueSubmit (queue, 1, &si, slot->fence);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vkQueueSubmit(): %s", vk_result_string (rc));

    return -1;
  }

  slot->submitted = 1;

  if (blocking == false)
  {
    if (k->timestamps_supported) slot->ts_pending = 1;
  }

  k->slot_next = (k->slot_next + 1) % HC_VK_SLOTS;

  if (blocking)
  {
    rc = vk->vkWaitForFences (k->device, 1, &slot->fence, VK_TRUE, 10ull * 1000ull * 1000ull * 1000ull);

    if (rc != VK_SUCCESS)
    {
      event_log_error (hashcat_ctx, "hc_vkDispatch(%s): %s after 10s (gx=%llu gy=%llu lsz=%u,%u)",
                       k->entrypoint ? k->entrypoint : "?", vk_result_string (rc),
                       (unsigned long long) gx, (unsigned long long) gy,
                       k->local_size_x, k->local_size_y);

      if (rc == VK_TIMEOUT)
      {
        // make the next submit possible; the queue itself may be wedged

        vk->vkQueueWaitIdle (queue);
      }

      return -1;
    }

    slot->submitted = 0;

    if (k->timestamps_supported && exec_ms)
    {
      vk_slot_read_timestamps (vk, k, slot, exec_ms);
    }
  }

  if (dbg)
  {
    fprintf (stderr, "[vk] done %s\n", k->entrypoint ? k->entrypoint : "?");
    fflush (stderr);
  }

  return 0;
}

// ---------------------------------------------------------------------------
// GPU transfer ops
//
// The backend's buffers are all host-visible, which allowed copy/fill to be
// implemented as plain CPU memcpy/memset. That serializes against the compute
// queue: the GPU sits idle for the whole duration of every large DtoD copy
// (most notably the pws_amp_buf -> pws_buf bounce in choose_kernel), which
// showed up as a sawtooth GPU load. vkCmdCopyBuffer / vkCmdFillBuffer move
// this work onto the device itself.
//
// HASHCAT_VK_SYNC=1 keeps the old CPU behavior (debugging / A-B tests).
// ---------------------------------------------------------------------------

static int vk_cpu_fallback (void)
{
  return (getenv ("HASHCAT_VK_SYNC") != NULL);
}

static int vk_copy_ctx_ensure (void *hashcat_ctx, hc_vk_buffer_t *buf)
{
  if (buf->copy_ready) return 0;

  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  int qfam = vk_find_compute_family (vk, buf->phys);

  if (qfam == -1)
  {
    event_log_error (hashcat_ctx, "vk_copy_ctx_ensure(): no compute queue found");

    return -1;
  }

  VkCommandPoolCreateInfo cpolci =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = (uint32_t) qfam
  };

  if (vk->vkCreateCommandPool (buf->device, &cpolci, NULL, &buf->copy_pool) != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vk_copy_ctx_ensure(): vkCreateCommandPool failed");

    return -1;
  }

  VkCommandBufferAllocateInfo cbai =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = buf->copy_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1
  };

  if (vk->vkAllocateCommandBuffers (buf->device, &cbai, &buf->copy_cmd) != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vk_copy_ctx_ensure(): vkAllocateCommandBuffers failed");

    return -1;
  }

  VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

  if (vk->vkCreateFence (buf->device, &fci, NULL, &buf->copy_fence) != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vk_copy_ctx_ensure(): vkCreateFence failed");

    return -1;
  }

  buf->copy_ready = 1;

  return 0;
}

// wait for the transfer issued previously on this buffer (entry point before
// rewriting the command buffer, and for hc_vkBufferFree). returns 0 on success.

static int vk_transfer_wait (void *hashcat_ctx, hc_vk_buffer_t *buf, const char *what)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if (buf->copy_pending == 0) return 0;

  VkResult rc = vk->vkWaitForFences (buf->device, 1, &buf->copy_fence, VK_TRUE, 10ull * 1000ull * 1000ull * 1000ull);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vk_transfer_wait(%s): %s after 10s", what, vk_result_string (rc));

    if (rc == VK_TIMEOUT)
    {
      int qfam = vk_find_compute_family (vk, buf->phys);

      if (qfam != -1)
      {
        VkQueue queue = VK_NULL_HANDLE;

        vk->vkGetDeviceQueue (buf->device, (uint32_t) qfam, 0, &queue);

        if (queue != VK_NULL_HANDLE) vk->vkQueueWaitIdle (queue);
      }
    }

    return -1;
  }

  buf->copy_pending = 0;

  return 0;
}

// submit the recorded transfer command buffer on the compute queue. callers do
// NOT wait for it: queue ordering covers all subsequent GPU work, and the next
// CPU operation that must see the result goes through hc_vkQueueIdle or
// vk_transfer_wait(). returns 0 on success.

static int vk_transfer_submit (void *hashcat_ctx, hc_vk_buffer_t *buf)
{
  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  int qfam = vk_find_compute_family (vk, buf->phys);

  if (qfam == -1)
  {
    event_log_error (hashcat_ctx, "vk_transfer_submit(): no compute queue found");

    return -1;
  }

  VkQueue queue = VK_NULL_HANDLE;

  vk->vkGetDeviceQueue (buf->device, (uint32_t) qfam, 0, &queue);

  if (queue == VK_NULL_HANDLE)
  {
    event_log_error (hashcat_ctx, "vk_transfer_submit(): vkGetDeviceQueue failed");

    return -1;
  }

  VkResult rc = vk->vkResetFences (buf->device, 1, &buf->copy_fence);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vk_transfer_submit(): vkResetFences(): %s", vk_result_string (rc));

    return -1;
  }

  VkSubmitInfo si =
  {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &buf->copy_cmd
  };

  rc = vk->vkQueueSubmit (queue, 1, &si, buf->copy_fence);

  if (rc != VK_SUCCESS)
  {
    event_log_error (hashcat_ctx, "vk_transfer_submit(): vkQueueSubmit(): %s", vk_result_string (rc));

    return -1;
  }

  buf->copy_pending = 1;

  return 0;
}

// transfer writes must be visible to every later consumer of the target buffer

static void vk_transfer_barrier (VK_PTR *vk, VkCommandBuffer cmd)
{
  VkMemoryBarrier mb =
  {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT   | VK_ACCESS_MEMORY_WRITE_BIT
  };

  vk->vkCmdPipelineBarrier (cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                            0, 1, &mb, 0, NULL, 0, NULL);
}

int hc_vkCopyBuffer (void *hashcat_ctx, hc_vk_buffer_t *src, hc_vk_buffer_t *dst, size_t src_off, size_t dst_off, size_t size)
{
  if (size == 0) return 0;

  // alignment or setup issues: the CPU path is correct, just slow

  const bool aligned = ((src_off % 4) == 0) && ((dst_off % 4) == 0) && ((size % 4) == 0);

  if (vk_cpu_fallback () || (aligned == false) || (dst->device != src->device) || (vk_copy_ctx_ensure (hashcat_ctx, dst) == -1))
  {
    memcpy ((char *) dst->host + dst_off, (const char *) src->host + src_off, size);

    return 0;
  }

  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if (vk_transfer_wait (hashcat_ctx, dst, "copy") == -1)
  {
    memcpy ((char *) dst->host + dst_off, (const char *) src->host + src_off, size);

    return 0;
  }

  vk->vkResetCommandBuffer (dst->copy_cmd, 0);

  VkCommandBufferBeginInfo bi =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  vk->vkBeginCommandBuffer (dst->copy_cmd, &bi);

  VkBufferCopy region = { .srcOffset = src_off, .dstOffset = dst_off, .size = size };

  vk->vkCmdCopyBuffer (dst->copy_cmd, src->buffer, dst->buffer, 1, &region);

  vk_transfer_barrier (vk, dst->copy_cmd);

  vk->vkEndCommandBuffer (dst->copy_cmd);

  if (vk_transfer_submit (hashcat_ctx, dst) == -1)
  {
    memcpy ((char *) dst->host + dst_off, (const char *) src->host + src_off, size);

    return 0;
  }

  return 0;
}

int hc_vkFillBuffer8 (void *hashcat_ctx, hc_vk_buffer_t *buf, size_t offset, uint8_t value, size_t size)
{
  if (size == 0) return 0;

  const bool aligned = ((offset % 4) == 0) && ((size % 4) == 0);

  if (vk_cpu_fallback () || (aligned == false) || (vk_copy_ctx_ensure (hashcat_ctx, buf) == -1))
  {
    memset ((char *) buf->host + offset, value, size);

    return 0;
  }

  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  const uint32_t pat = ((uint32_t) value << 24) | ((uint32_t) value << 16) | ((uint32_t) value << 8) | (uint32_t) value;

  if (vk_transfer_wait (hashcat_ctx, buf, "fill8") == -1)
  {
    memset ((char *) buf->host + offset, value, size);

    return 0;
  }

  vk->vkResetCommandBuffer (buf->copy_cmd, 0);

  VkCommandBufferBeginInfo bi =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  vk->vkBeginCommandBuffer (buf->copy_cmd, &bi);

  vk->vkCmdFillBuffer (buf->copy_cmd, buf->buffer, offset, size, pat);

  vk_transfer_barrier (vk, buf->copy_cmd);

  vk->vkEndCommandBuffer (buf->copy_cmd);

  if (vk_transfer_submit (hashcat_ctx, buf) == -1)
  {
    memset ((char *) buf->host + offset, value, size);

    return 0;
  }

  return 0;
}

int hc_vkFillBuffer32 (void *hashcat_ctx, hc_vk_buffer_t *buf, size_t offset, uint32_t value, size_t size)
{
  if (size == 0) return 0;

  const bool aligned = ((offset % 4) == 0) && ((size % 4) == 0);

  if (vk_cpu_fallback () || (aligned == false) || (vk_copy_ctx_ensure (hashcat_ctx, buf) == -1))
  {
    uint32_t *p = (uint32_t *) ((char *) buf->host + offset);

    size_t cnt = size / sizeof (uint32_t);

    for (size_t i = 0; i < cnt; i++) p[i] = value;

    return 0;
  }

  VK_PTR *vk = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx->vk;

  if (vk_transfer_wait (hashcat_ctx, buf, "fill32") == -1)
  {
    uint32_t *p = (uint32_t *) ((char *) buf->host + offset);

    size_t cnt = size / sizeof (uint32_t);

    for (size_t i = 0; i < cnt; i++) p[i] = value;

    return 0;
  }

  vk->vkResetCommandBuffer (buf->copy_cmd, 0);

  VkCommandBufferBeginInfo bi =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  vk->vkBeginCommandBuffer (buf->copy_cmd, &bi);

  vk->vkCmdFillBuffer (buf->copy_cmd, buf->buffer, offset, size, value);

  vk_transfer_barrier (vk, buf->copy_cmd);

  vk->vkEndCommandBuffer (buf->copy_cmd);

  if (vk_transfer_submit (hashcat_ctx, buf) == -1)
  {
    uint32_t *p = (uint32_t *) ((char *) buf->host + offset);

    size_t cnt = size / sizeof (uint32_t);

    for (size_t i = 0; i < cnt; i++) p[i] = value;

    return 0;
  }

  return 0;
}
