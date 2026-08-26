/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#ifndef HC_EXT_VULKAN_H
#define HC_EXT_VULKAN_H

#include "export.h"

#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>

// number of descriptor bindings we expose per kernel.
// the largest hashcat kernel takes 28 arguments (PCFG), round up.

#define HC_VK_MAX_BINDINGS 32

typedef struct hc_vk_buffer
{
  VkBuffer       buffer;
  VkDeviceMemory mem;
  void          *host;   // persistent mapping, host coherent
  VkDeviceSize   size;
  VkDevice       device;
  VkPhysicalDevice phys;

  // lazily created GPU transfer context for vkCmdCopyBuffer / vkCmdFillBuffer
  // on this buffer (HASHCAT_VK_SYNC=1 falls back to the CPU path)

  VkCommandPool    copy_pool;
  VkCommandBuffer  copy_cmd;
  VkFence          copy_fence;
  int              copy_ready;
  int              copy_pending;   // a GPU transfer is in flight on this buffer

} hc_vk_buffer_t;

typedef struct hc_vk_slot
{
  VkCommandBuffer cmdbuf;
  VkFence         fence;
  int             submitted;   // fence may still be pending
  int             ts_pending;  // this slot submitted a timestamp pair not yet read

  // slot-owned descriptor set: a dispatch about to reuse this slot has already
  // waited on its fence, so its set can be rewritten without touching the
  // other slot's in-flight work

  VkDescriptorSet dset;
  int             dset_valid;

  // slot-owned copy of the caller's scalar/POD argument buffer: the buffer the
  // caller hands out (vk_d_kernel_param) is overwritten by the next launch
  // while this dispatch may still be reading it, so every submission reads a
  // stable private snapshot

  hc_vk_buffer_t  pod_buf;
  int             pod_ready;

} hc_vk_slot_t;

// dispatches use two command buffers in rotation: the host records the next
// launch while the GPU is still chewing on the previous one (the old backend
// blocked on a fence for every single kernel, which starved the GPU between
// launches). slot fences are the only synchronization a dispatch ever waits
// on, and only the fence of the slot it is about to reuse.
//
// each slot owns two timestamp queries, so a pipelined dispatch feeds the
// caller's exec_ms with the (already finished) previous run of this kernel
// instead of blocking. both slots' data stay readable; only the reset of the
// pool slice about to be re-recorded happens inside the command buffer.

#define HC_VK_SLOTS 2

typedef struct hc_vk_kernel
{
  // fixed objects, created once per kernel slot

  VkDevice               device;
  VkPhysicalDevice       phys;
  VkDescriptorSetLayout  dslayout;
  VkDescriptorPool       pool;
  VkPipelineLayout       layout;

  VkCommandPool          cmdpool;

  hc_vk_slot_t           slots[HC_VK_SLOTS];
  int                    slot_next;

  VkQueryPool            querypool;
  double                 timestamp_period;
  int                    timestamps_supported;

  // per compiled entry point

  VkPipeline             pipeline;
  char                  *entrypoint;

  uint32_t               local_size_x;
  uint32_t               local_size_y;
  uint32_t               local_size_z;

  // ordinal -> descriptor binding map from the clspv reflection metadata.
  // clspv eliminates unused kernel arguments, so bindings are compacted and
  // do not match hashcat's argument indices. -1 means the argument was
  // eliminated and SetKernelArg on it is a no-op.

  int                    ord_to_binding[HC_VK_MAX_BINDINGS];

  // scalar arguments live in the push constant interface. per ordinal:
  // byte offset and size inside the push constant block, -1 when absent.
  // values are read from the caller's staging buffer (vk_d_kernel_param)
  // at dispatch and pushed via vkCmdPushConstants.

  int                    pod_offset[HC_VK_MAX_BINDINGS];
  int                    pod_size[HC_VK_MAX_BINDINGS];

  // buffers currently bound, indexed by ARGUMENT ORDINAL

  VkBuffer               bindings[HC_VK_MAX_BINDINGS];
  int                    dirty;

} hc_vk_kernel_t;

// reflection info extracted from one SPIR-V module

typedef struct hc_vk_refl_entry
{
  char name[128];
  int  ord_to_binding[HC_VK_MAX_BINDINGS];
  int  pod_offset[HC_VK_MAX_BINDINGS];
  int  pod_size[HC_VK_MAX_BINDINGS];

} hc_vk_refl_entry_t;

typedef struct hc_vk_refl
{
  hc_vk_refl_entry_t *entries;
  uint32_t            count;

} hc_vk_refl_t;

typedef hc_vk_kernel_t *vk_kernel;
typedef hc_vk_buffer_t *vk_buffer;
typedef void           *vk_program; // VkShaderModule

typedef struct hc_vulkan_lib
{
  hc_dynlib_t lib;

  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkCreateInstance      vkCreateInstance;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkGetDeviceQueue vkGetDeviceQueue;
  PFN_vkDestroyInstance vkDestroyInstance;
  PFN_vkDestroyDevice vkDestroyDevice;

  PFN_vkCreateShaderModule vkCreateShaderModule;
  PFN_vkDestroyShaderModule vkDestroyShaderModule;
  PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
  PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
  PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
  PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
  PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
  PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
  PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
  PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
  PFN_vkCreateComputePipelines vkCreateComputePipelines;
  PFN_vkDestroyPipeline vkDestroyPipeline;
  PFN_vkCreateCommandPool vkCreateCommandPool;
  PFN_vkDestroyCommandPool vkDestroyCommandPool;
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
  PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
  PFN_vkResetCommandPool vkResetCommandPool;
  PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
  PFN_vkEndCommandBuffer vkEndCommandBuffer;
  PFN_vkResetCommandBuffer vkResetCommandBuffer;
  PFN_vkCmdBindPipeline vkCmdBindPipeline;
  PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
  PFN_vkCmdPushConstants vkCmdPushConstants;
  PFN_vkCmdDispatch vkCmdDispatch;
  PFN_vkCmdFillBuffer vkCmdFillBuffer;
  PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
  PFN_vkQueueSubmit vkQueueSubmit;
  PFN_vkQueueWaitIdle vkQueueWaitIdle;
  PFN_vkCreateFence vkCreateFence;
  PFN_vkDestroyFence vkDestroyFence;
  PFN_vkResetFences vkResetFences;
  PFN_vkWaitForFences vkWaitForFences;
  PFN_vkCreateQueryPool vkCreateQueryPool;
  PFN_vkDestroyQueryPool vkDestroyQueryPool;
  PFN_vkResetQueryPool vkResetQueryPool;
  PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
  PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
  PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
  PFN_vkCreateBuffer vkCreateBuffer;
  PFN_vkDestroyBuffer vkDestroyBuffer;
  PFN_vkAllocateMemory vkAllocateMemory;
  PFN_vkFreeMemory vkFreeMemory;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
  PFN_vkBindBufferMemory vkBindBufferMemory;
  PFN_vkMapMemory vkMapMemory;
  PFN_vkUnmapMemory vkUnmapMemory;
  PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;

  // async queueing is enabled only around the attack loop's choose_kernel() call:
  // selftest, autotune, bridges and other entry points stage and read data through
  // the mapped buffers with no synchronization, so they keep blocking dispatches.

  int async_enabled;

} hc_vulkan_lib_t;

typedef hc_vulkan_lib_t VK_PTR;

int  vk_init (void *hashcat_ctx);
void vk_close (void *hashcat_ctx);

int hc_vkEnumeratePhysicalDevices (void *hashcat_ctx, VkInstance instance, uint32_t *count, VkPhysicalDevice *devices);
int hc_vkGetPhysicalDeviceProperties (void *hashcat_ctx, VkPhysicalDevice dev, VkPhysicalDeviceProperties *props);
int hc_vkCreateDevice (void *hashcat_ctx, VkInstance instance, VkPhysicalDevice dev, VkDevice *device, uint32_t *queue_family);

int hc_vkBufferAlloc (void *hashcat_ctx, VkDevice device, VkPhysicalDevice phys, hc_vk_buffer_t *buf, VkDeviceSize size);
int hc_vkBufferFree (void *hashcat_ctx, hc_vk_buffer_t *buf);

int hc_vkProgramCreate (void *hashcat_ctx, VkDevice device, const char *spirv, size_t spirv_size, vk_program *program, hc_vk_refl_t **refl);
void hc_vkReflFree (void *hashcat_ctx, hc_vk_refl_t *refl);
int hc_vkProgramDestroy (void *hashcat_ctx, VkDevice device, vk_program program);

int hc_vkKernelInit (void *hashcat_ctx, VkDevice device, VkPhysicalDevice phys, vk_kernel *kernel);
int hc_vkKernelTerm (void *hashcat_ctx, vk_kernel kernel);
int hc_vkKernelCompile (void *hashcat_ctx, vk_kernel kernel, vk_program program, hc_vk_refl_t *refl, const char *entrypoint, uint32_t lszx, uint32_t lszy);
int hc_vkSetKernelArg (void *hashcat_ctx, vk_kernel kernel, uint32_t index, hc_vk_buffer_t *value);

// blocking dispatch: returns after the kernel has finished, exec_ms receives GPU time.
// when HASHCAT_VK_SYNC is not set and exec_ms is NULL, the dispatch is pipelined:
// it only waits for the fence of the command-buffer slot it reuses, and callers that
// read device-written data must go through hc_vkQueueIdle first.

int hc_vkQueueIdle (void *hashcat_ctx, VkQueue queue);

int hc_vkDispatch (void *hashcat_ctx, VkQueue queue, vk_kernel kernel,
                   uint64_t gx, uint64_t gy, hc_vk_buffer_t *pod_buffer, double *exec_ms);

// native fills, host side (buffers are persistently mapped)

int hc_vkFillBuffer8  (void *hashcat_ctx, hc_vk_buffer_t *buf, size_t offset, uint8_t  value, size_t size);
int hc_vkFillBuffer32 (void *hashcat_ctx, hc_vk_buffer_t *buf, size_t offset, uint32_t value, size_t size);
int hc_vkCopyBuffer (void *hashcat_ctx, hc_vk_buffer_t *src, hc_vk_buffer_t *dst, size_t src_off, size_t dst_off, size_t size);

#endif // HC_EXT_VULKAN_H
