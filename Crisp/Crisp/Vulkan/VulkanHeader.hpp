#pragma once

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <vulkan/vulkan.h>

#include <type_traits>

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(VK_VERSION_1_0)
    extern PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    extern PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    extern PFN_vkAllocateMemory vkAllocateMemory;
    extern PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    extern PFN_vkBindBufferMemory vkBindBufferMemory;
    extern PFN_vkBindImageMemory vkBindImageMemory;
    extern PFN_vkCmdBeginQuery vkCmdBeginQuery;
    extern PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    extern PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    extern PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
    extern PFN_vkCmdBindPipeline vkCmdBindPipeline;
    extern PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    extern PFN_vkCmdBlitImage vkCmdBlitImage;
    extern PFN_vkCmdClearAttachments vkCmdClearAttachments;
    extern PFN_vkCmdClearColorImage vkCmdClearColorImage;
    extern PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
    extern PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    extern PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
    extern PFN_vkCmdCopyImage vkCmdCopyImage;
    extern PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
    extern PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
    extern PFN_vkCmdDispatch vkCmdDispatch;
    extern PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
    extern PFN_vkCmdDraw vkCmdDraw;
    extern PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
    extern PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
    extern PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
    extern PFN_vkCmdEndQuery vkCmdEndQuery;
    extern PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
    extern PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
    extern PFN_vkCmdFillBuffer vkCmdFillBuffer;
    extern PFN_vkCmdNextSubpass vkCmdNextSubpass;
    extern PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    extern PFN_vkCmdPushConstants vkCmdPushConstants;
    extern PFN_vkCmdResetEvent vkCmdResetEvent;
    extern PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
    extern PFN_vkCmdResolveImage vkCmdResolveImage;
    extern PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
    extern PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
    extern PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
    extern PFN_vkCmdSetEvent vkCmdSetEvent;
    extern PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
    extern PFN_vkCmdSetScissor vkCmdSetScissor;
    extern PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
    extern PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
    extern PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
    extern PFN_vkCmdSetViewport vkCmdSetViewport;
    extern PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
    extern PFN_vkCmdWaitEvents vkCmdWaitEvents;
    extern PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
    extern PFN_vkCreateBuffer vkCreateBuffer;
    extern PFN_vkCreateBufferView vkCreateBufferView;
    extern PFN_vkCreateCommandPool vkCreateCommandPool;
    extern PFN_vkCreateComputePipelines vkCreateComputePipelines;
    extern PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    extern PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    extern PFN_vkCreateDevice vkCreateDevice;
    extern PFN_vkCreateEvent vkCreateEvent;
    extern PFN_vkCreateFence vkCreateFence;
    extern PFN_vkCreateFramebuffer vkCreateFramebuffer;
    extern PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    extern PFN_vkCreateImage vkCreateImage;
    extern PFN_vkCreateImageView vkCreateImageView;
    extern PFN_vkCreateInstance vkCreateInstance;
    extern PFN_vkCreatePipelineCache vkCreatePipelineCache;
    extern PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    extern PFN_vkCreateQueryPool vkCreateQueryPool;
    extern PFN_vkCreateRenderPass vkCreateRenderPass;
    extern PFN_vkCreateSampler vkCreateSampler;
    extern PFN_vkCreateSemaphore vkCreateSemaphore;
    extern PFN_vkCreateShaderModule vkCreateShaderModule;
    extern PFN_vkDestroyBuffer vkDestroyBuffer;
    extern PFN_vkDestroyBufferView vkDestroyBufferView;
    extern PFN_vkDestroyCommandPool vkDestroyCommandPool;
    extern PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    extern PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    extern PFN_vkDestroyDevice vkDestroyDevice;
    extern PFN_vkDestroyEvent vkDestroyEvent;
    extern PFN_vkDestroyFence vkDestroyFence;
    extern PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    extern PFN_vkDestroyImage vkDestroyImage;
    extern PFN_vkDestroyImageView vkDestroyImageView;
    extern PFN_vkDestroyInstance vkDestroyInstance;
    extern PFN_vkDestroyPipeline vkDestroyPipeline;
    extern PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
    extern PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    extern PFN_vkDestroyQueryPool vkDestroyQueryPool;
    extern PFN_vkDestroyRenderPass vkDestroyRenderPass;
    extern PFN_vkDestroySampler vkDestroySampler;
    extern PFN_vkDestroySemaphore vkDestroySemaphore;
    extern PFN_vkDestroyShaderModule vkDestroyShaderModule;
    extern PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    extern PFN_vkEndCommandBuffer vkEndCommandBuffer;
    extern PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
    extern PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
    extern PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
    extern PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    extern PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    extern PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    extern PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    extern PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    extern PFN_vkFreeMemory vkFreeMemory;
    extern PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    extern PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
    extern PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    extern PFN_vkGetDeviceQueue vkGetDeviceQueue;
    extern PFN_vkGetEventStatus vkGetEventStatus;
    extern PFN_vkGetFenceStatus vkGetFenceStatus;
    extern PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    extern PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
    extern PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
    extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    extern PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
    extern PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
    extern PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
    extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    extern PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    extern PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
    extern PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
    extern PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
    extern PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
    extern PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
    extern PFN_vkMapMemory vkMapMemory;
    extern PFN_vkMergePipelineCaches vkMergePipelineCaches;
    extern PFN_vkQueueBindSparse vkQueueBindSparse;
    extern PFN_vkQueueSubmit vkQueueSubmit;
    extern PFN_vkQueueWaitIdle vkQueueWaitIdle;
    extern PFN_vkResetCommandBuffer vkResetCommandBuffer;
    extern PFN_vkResetCommandPool vkResetCommandPool;
    extern PFN_vkResetDescriptorPool vkResetDescriptorPool;
    extern PFN_vkResetEvent vkResetEvent;
    extern PFN_vkResetFences vkResetFences;
    extern PFN_vkSetEvent vkSetEvent;
    extern PFN_vkUnmapMemory vkUnmapMemory;
    extern PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    extern PFN_vkWaitForFences vkWaitForFences;
#endif

#if defined(VK_VERSION_1_1)
    extern PFN_vkBindBufferMemory2 vkBindBufferMemory2;
    extern PFN_vkBindImageMemory2 vkBindImageMemory2;
    extern PFN_vkCmdDispatchBase vkCmdDispatchBase;
    extern PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
    extern PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
    extern PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
    extern PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
    extern PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
    extern PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
    extern PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
    extern PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
    extern PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
    extern PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
    extern PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
    extern PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
    extern PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
    extern PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
    extern PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
    extern PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
    extern PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
    extern PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
    extern PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
    extern PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
    extern PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
    extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
    extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 vkGetPhysicalDeviceSparseImageFormatProperties2;
    extern PFN_vkTrimCommandPool vkTrimCommandPool;
    extern PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#endif

#if defined(VK_VERSION_1_2)
    extern PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2;
    extern PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount;
    extern PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount;
    extern PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2;
    extern PFN_vkCmdNextSubpass2 vkCmdNextSubpass2;
    extern PFN_vkCreateRenderPass2 vkCreateRenderPass2;
    extern PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    extern PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress;
    extern PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress;
    extern PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
    extern PFN_vkResetQueryPool vkResetQueryPool;
    extern PFN_vkSignalSemaphore vkSignalSemaphore;
    extern PFN_vkWaitSemaphores vkWaitSemaphores;
#endif

#if defined(VK_VERSION_1_3)
    extern PFN_vkCmdBeginRendering vkCmdBeginRendering;
    extern PFN_vkCmdBindVertexBuffers2 vkCmdBindVertexBuffers2;
    extern PFN_vkCmdBlitImage2 vkCmdBlitImage2;
    extern PFN_vkCmdCopyBuffer2 vkCmdCopyBuffer2;
    extern PFN_vkCmdCopyBufferToImage2 vkCmdCopyBufferToImage2;
    extern PFN_vkCmdCopyImage2 vkCmdCopyImage2;
    extern PFN_vkCmdCopyImageToBuffer2 vkCmdCopyImageToBuffer2;
    extern PFN_vkCmdEndRendering vkCmdEndRendering;
    extern PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2;
    extern PFN_vkCmdResetEvent2 vkCmdResetEvent2;
    extern PFN_vkCmdResolveImage2 vkCmdResolveImage2;
    extern PFN_vkCmdSetCullMode vkCmdSetCullMode;
    extern PFN_vkCmdSetDepthBiasEnable vkCmdSetDepthBiasEnable;
    extern PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable;
    extern PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp;
    extern PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable;
    extern PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable;
    extern PFN_vkCmdSetEvent2 vkCmdSetEvent2;
    extern PFN_vkCmdSetFrontFace vkCmdSetFrontFace;
    extern PFN_vkCmdSetPrimitiveRestartEnable vkCmdSetPrimitiveRestartEnable;
    extern PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology;
    extern PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnable;
    extern PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount;
    extern PFN_vkCmdSetStencilOp vkCmdSetStencilOp;
    extern PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable;
    extern PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount;
    extern PFN_vkCmdWaitEvents2 vkCmdWaitEvents2;
    extern PFN_vkCmdWriteTimestamp2 vkCmdWriteTimestamp2;
    extern PFN_vkCreatePrivateDataSlot vkCreatePrivateDataSlot;
    extern PFN_vkDestroyPrivateDataSlot vkDestroyPrivateDataSlot;
    extern PFN_vkGetDeviceBufferMemoryRequirements vkGetDeviceBufferMemoryRequirements;
    extern PFN_vkGetDeviceImageMemoryRequirements vkGetDeviceImageMemoryRequirements;
    extern PFN_vkGetDeviceImageSparseMemoryRequirements vkGetDeviceImageSparseMemoryRequirements;
    extern PFN_vkGetPhysicalDeviceToolProperties vkGetPhysicalDeviceToolProperties;
    extern PFN_vkGetPrivateData vkGetPrivateData;
    extern PFN_vkQueueSubmit2 vkQueueSubmit2;
    extern PFN_vkSetPrivateData vkSetPrivateData;
#endif

#if defined(VK_AMD_buffer_marker)
    extern PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif

#if defined(VK_AMD_display_native_hdr)
    extern PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif

#if defined(VK_AMD_draw_indirect_count)
    extern PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
    extern PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#endif

#if defined(VK_AMD_shader_info)
    extern PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
    extern PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
    extern PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif

#if defined(VK_EXT_acquire_drm_display)
    extern PFN_vkAcquireDrmDisplayEXT vkAcquireDrmDisplayEXT;
    extern PFN_vkGetDrmDisplayEXT vkGetDrmDisplayEXT;
#endif

#if defined(VK_EXT_acquire_xlib_display)
    extern PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
    extern PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif

#if defined(VK_EXT_buffer_device_address)
    extern PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif

#if defined(VK_EXT_calibrated_timestamps)
    extern PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
    extern PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif

#if defined(VK_EXT_color_write_enable)
    extern PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
#endif

#if defined(VK_EXT_conditional_rendering)
    extern PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
    extern PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif

#if defined(VK_EXT_debug_marker)
    extern PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
    extern PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
    extern PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
    extern PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
    extern PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#endif

#if defined(VK_EXT_debug_report)
    extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    extern PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
    extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
#endif

#if defined(VK_EXT_debug_utils)
    extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
    extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
    extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    extern PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
    extern PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
    extern PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
    extern PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
    extern PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
    extern PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
#endif

#if defined(VK_EXT_descriptor_buffer)
    extern PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT vkCmdBindDescriptorBufferEmbeddedSamplersEXT;
    extern PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT;
    extern PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT;
    extern PFN_vkGetBufferOpaqueCaptureDescriptorDataEXT vkGetBufferOpaqueCaptureDescriptorDataEXT;
    extern PFN_vkGetDescriptorEXT vkGetDescriptorEXT;
    extern PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffsetEXT;
    extern PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSizeEXT;
    extern PFN_vkGetImageOpaqueCaptureDescriptorDataEXT vkGetImageOpaqueCaptureDescriptorDataEXT;
    extern PFN_vkGetImageViewOpaqueCaptureDescriptorDataEXT vkGetImageViewOpaqueCaptureDescriptorDataEXT;
    extern PFN_vkGetSamplerOpaqueCaptureDescriptorDataEXT vkGetSamplerOpaqueCaptureDescriptorDataEXT;
#endif

#if defined(VK_EXT_descriptor_buffer) && defined(VK_KHR_acceleration_structure) && defined(VK_NV_ray_tracing)
    extern PFN_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT
        vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT;
#endif

#if defined(VK_EXT_device_fault)
    extern PFN_vkGetDeviceFaultInfoEXT vkGetDeviceFaultInfoEXT;
#endif

#if defined(VK_EXT_direct_mode_display)
    extern PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#endif

#if defined(VK_EXT_directfb_surface)
    extern PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
    extern PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif

#if defined(VK_EXT_discard_rectangles)
    extern PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif

#if defined(VK_EXT_display_control)
    extern PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
    extern PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
    extern PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
    extern PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#endif

#if defined(VK_EXT_display_surface_counter)
    extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#endif

#if defined(VK_EXT_extended_dynamic_state)
    extern PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
    extern PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
    extern PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
    extern PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
    extern PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
    extern PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
    extern PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
    extern PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
    extern PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
    extern PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
    extern PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
    extern PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;
#endif

#if defined(VK_EXT_extended_dynamic_state2)
    extern PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
    extern PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
    extern PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
    extern PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
    extern PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
#endif

#if defined(VK_EXT_extended_dynamic_state3)
    extern PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT;
    extern PFN_vkCmdSetAlphaToOneEnableEXT vkCmdSetAlphaToOneEnableEXT;
    extern PFN_vkCmdSetColorBlendAdvancedEXT vkCmdSetColorBlendAdvancedEXT;
    extern PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT;
    extern PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT;
    extern PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;
    extern PFN_vkCmdSetConservativeRasterizationModeEXT vkCmdSetConservativeRasterizationModeEXT;
    extern PFN_vkCmdSetCoverageModulationModeNV vkCmdSetCoverageModulationModeNV;
    extern PFN_vkCmdSetCoverageModulationTableEnableNV vkCmdSetCoverageModulationTableEnableNV;
    extern PFN_vkCmdSetCoverageModulationTableNV vkCmdSetCoverageModulationTableNV;
    extern PFN_vkCmdSetCoverageReductionModeNV vkCmdSetCoverageReductionModeNV;
    extern PFN_vkCmdSetCoverageToColorEnableNV vkCmdSetCoverageToColorEnableNV;
    extern PFN_vkCmdSetCoverageToColorLocationNV vkCmdSetCoverageToColorLocationNV;
    extern PFN_vkCmdSetDepthClampEnableEXT vkCmdSetDepthClampEnableEXT;
    extern PFN_vkCmdSetDepthClipEnableEXT vkCmdSetDepthClipEnableEXT;
    extern PFN_vkCmdSetDepthClipNegativeOneToOneEXT vkCmdSetDepthClipNegativeOneToOneEXT;
    extern PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT vkCmdSetExtraPrimitiveOverestimationSizeEXT;
    extern PFN_vkCmdSetLineRasterizationModeEXT vkCmdSetLineRasterizationModeEXT;
    extern PFN_vkCmdSetLineStippleEnableEXT vkCmdSetLineStippleEnableEXT;
    extern PFN_vkCmdSetLogicOpEnableEXT vkCmdSetLogicOpEnableEXT;
    extern PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;
    extern PFN_vkCmdSetProvokingVertexModeEXT vkCmdSetProvokingVertexModeEXT;
    extern PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT;
    extern PFN_vkCmdSetRasterizationStreamEXT vkCmdSetRasterizationStreamEXT;
    extern PFN_vkCmdSetRepresentativeFragmentTestEnableNV vkCmdSetRepresentativeFragmentTestEnableNV;
    extern PFN_vkCmdSetSampleLocationsEnableEXT vkCmdSetSampleLocationsEnableEXT;
    extern PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT;
    extern PFN_vkCmdSetShadingRateImageEnableNV vkCmdSetShadingRateImageEnableNV;
    extern PFN_vkCmdSetTessellationDomainOriginEXT vkCmdSetTessellationDomainOriginEXT;
    extern PFN_vkCmdSetViewportSwizzleNV vkCmdSetViewportSwizzleNV;
    extern PFN_vkCmdSetViewportWScalingEnableNV vkCmdSetViewportWScalingEnableNV;
#endif

#if defined(VK_EXT_external_memory_host)
    extern PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif

#if defined(VK_EXT_full_screen_exclusive)
    extern PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
    extern PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
    extern PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#endif

#if defined(VK_EXT_hdr_metadata)
    extern PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif

#if defined(VK_EXT_headless_surface)
    extern PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#endif

#if defined(VK_EXT_host_query_reset)
    extern PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif

#if defined(VK_EXT_image_compression_control)
    extern PFN_vkGetImageSubresourceLayout2EXT vkGetImageSubresourceLayout2EXT;
#endif

#if defined(VK_EXT_image_drm_format_modifier)
    extern PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif

#if defined(VK_EXT_line_rasterization)
    extern PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif

#if defined(VK_EXT_mesh_shader)
    extern PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
    extern PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;
    extern PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXT;
#endif

#if defined(VK_EXT_metal_objects)
    extern PFN_vkExportMetalObjectsEXT vkExportMetalObjectsEXT;
#endif

#if defined(VK_EXT_metal_surface)
    extern PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif

#if defined(VK_EXT_multi_draw)
    extern PFN_vkCmdDrawMultiEXT vkCmdDrawMultiEXT;
    extern PFN_vkCmdDrawMultiIndexedEXT vkCmdDrawMultiIndexedEXT;
#endif

#if defined(VK_EXT_opacity_micromap)
    extern PFN_vkBuildMicromapsEXT vkBuildMicromapsEXT;
    extern PFN_vkCmdBuildMicromapsEXT vkCmdBuildMicromapsEXT;
    extern PFN_vkCmdCopyMemoryToMicromapEXT vkCmdCopyMemoryToMicromapEXT;
    extern PFN_vkCmdCopyMicromapEXT vkCmdCopyMicromapEXT;
    extern PFN_vkCmdCopyMicromapToMemoryEXT vkCmdCopyMicromapToMemoryEXT;
    extern PFN_vkCmdWriteMicromapsPropertiesEXT vkCmdWriteMicromapsPropertiesEXT;
    extern PFN_vkCopyMemoryToMicromapEXT vkCopyMemoryToMicromapEXT;
    extern PFN_vkCopyMicromapEXT vkCopyMicromapEXT;
    extern PFN_vkCopyMicromapToMemoryEXT vkCopyMicromapToMemoryEXT;
    extern PFN_vkCreateMicromapEXT vkCreateMicromapEXT;
    extern PFN_vkDestroyMicromapEXT vkDestroyMicromapEXT;
    extern PFN_vkGetDeviceMicromapCompatibilityEXT vkGetDeviceMicromapCompatibilityEXT;
    extern PFN_vkGetMicromapBuildSizesEXT vkGetMicromapBuildSizesEXT;
    extern PFN_vkWriteMicromapsPropertiesEXT vkWriteMicromapsPropertiesEXT;
#endif /* defined(VK_EXT_opacity_micromap) */
#if defined(VK_EXT_pageable_device_local_memory)
    extern PFN_vkSetDeviceMemoryPriorityEXT vkSetDeviceMemoryPriorityEXT;
#endif

#if defined(VK_EXT_pipeline_properties)
    extern PFN_vkGetPipelinePropertiesEXT vkGetPipelinePropertiesEXT;
#endif

#if defined(VK_EXT_private_data)
    extern PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
    extern PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
    extern PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
    extern PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
#endif

#if defined(VK_EXT_sample_locations)
    extern PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
    extern PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif

#if defined(VK_EXT_shader_module_identifier)
    extern PFN_vkGetShaderModuleCreateInfoIdentifierEXT vkGetShaderModuleCreateInfoIdentifierEXT;
    extern PFN_vkGetShaderModuleIdentifierEXT vkGetShaderModuleIdentifierEXT;
#endif

#if defined(VK_EXT_swapchain_maintenance1)
    extern PFN_vkReleaseSwapchainImagesEXT vkReleaseSwapchainImagesEXT;
#endif

#if defined(VK_EXT_tooling_info)
    extern PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
#endif

#if defined(VK_EXT_transform_feedback)
    extern PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
    extern PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
    extern PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
    extern PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
    extern PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
    extern PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#endif

#if defined(VK_EXT_validation_cache)
    extern PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
    extern PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
    extern PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
    extern PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#endif

#if defined(VK_EXT_vertex_input_dynamic_state)
    extern PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
#endif

#if defined(VK_FUCHSIA_buffer_collection)
    extern PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIA;
    extern PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIA;
    extern PFN_vkGetBufferCollectionPropertiesFUCHSIA vkGetBufferCollectionPropertiesFUCHSIA;
    extern PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA vkSetBufferCollectionBufferConstraintsFUCHSIA;
    extern PFN_vkSetBufferCollectionImageConstraintsFUCHSIA vkSetBufferCollectionImageConstraintsFUCHSIA;
#endif

#if defined(VK_FUCHSIA_external_memory)
    extern PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
    extern PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#endif

#if defined(VK_FUCHSIA_external_semaphore)
    extern PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
    extern PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
#endif

#if defined(VK_FUCHSIA_imagepipe_surface)
    extern PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif

#if defined(VK_GGP_stream_descriptor_surface)
    extern PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif

#if defined(VK_GOOGLE_display_timing)
    extern PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
    extern PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#endif

#if defined(VK_HUAWEI_cluster_culling_shader)
    extern PFN_vkCmdDrawClusterHUAWEI vkCmdDrawClusterHUAWEI;
    extern PFN_vkCmdDrawClusterIndirectHUAWEI vkCmdDrawClusterIndirectHUAWEI;
#endif

#if defined(VK_HUAWEI_invocation_mask)
    extern PFN_vkCmdBindInvocationMaskHUAWEI vkCmdBindInvocationMaskHUAWEI;
#endif

#if defined(VK_HUAWEI_subpass_shading)
    extern PFN_vkCmdSubpassShadingHUAWEI vkCmdSubpassShadingHUAWEI;
    extern PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
#endif

#if defined(VK_INTEL_performance_query)
    extern PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
    extern PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
    extern PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
    extern PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
    extern PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
    extern PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
    extern PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
    extern PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
    extern PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
#endif

#if defined(VK_KHR_acceleration_structure)
    extern PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
    extern PFN_vkCmdBuildAccelerationStructuresIndirectKHR vkCmdBuildAccelerationStructuresIndirectKHR;
    extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    extern PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
    extern PFN_vkCmdCopyAccelerationStructureToMemoryKHR vkCmdCopyAccelerationStructureToMemoryKHR;
    extern PFN_vkCmdCopyMemoryToAccelerationStructureKHR vkCmdCopyMemoryToAccelerationStructureKHR;
    extern PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
    extern PFN_vkCopyAccelerationStructureKHR vkCopyAccelerationStructureKHR;
    extern PFN_vkCopyAccelerationStructureToMemoryKHR vkCopyAccelerationStructureToMemoryKHR;
    extern PFN_vkCopyMemoryToAccelerationStructureKHR vkCopyMemoryToAccelerationStructureKHR;
    extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    extern PFN_vkGetDeviceAccelerationStructureCompatibilityKHR vkGetDeviceAccelerationStructureCompatibilityKHR;
    extern PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
#endif

#if defined(VK_KHR_android_surface)
    extern PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif

#if defined(VK_KHR_bind_memory2)
    extern PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
    extern PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif

#if defined(VK_KHR_buffer_device_address)
    extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    extern PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
    extern PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif

#if defined(VK_KHR_copy_commands2)
    extern PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
    extern PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
    extern PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
    extern PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
    extern PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
    extern PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
#endif

#if defined(VK_KHR_create_renderpass2)
    extern PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
    extern PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
    extern PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
    extern PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#endif

#if defined(VK_KHR_deferred_host_operations)
    extern PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
    extern PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
    extern PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
    extern PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
    extern PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
#endif

#if defined(VK_KHR_descriptor_update_template)
    extern PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
    extern PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
    extern PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#endif

#if defined(VK_KHR_device_group)
    extern PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
    extern PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
    extern PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#endif

#if defined(VK_KHR_device_group_creation)
    extern PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#endif

#if defined(VK_KHR_display)
    extern PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
    extern PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
    extern PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
    extern PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
    extern PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
    extern PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
    extern PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
#endif

#if defined(VK_KHR_display_swapchain)
    extern PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif

#if defined(VK_KHR_draw_indirect_count)
    extern PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
    extern PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#endif

#if defined(VK_KHR_dynamic_rendering)
    extern PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    extern PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
#endif

#if defined(VK_KHR_external_fence_capabilities)
    extern PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#endif

#if defined(VK_KHR_external_fence_fd)
    extern PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
    extern PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#endif

#if defined(VK_KHR_external_fence_win32)
    extern PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
    extern PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#endif

#if defined(VK_KHR_external_memory_capabilities)
    extern PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#endif

#if defined(VK_KHR_external_memory_fd)
    extern PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
    extern PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif

#if defined(VK_KHR_external_memory_win32)
    extern PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
    extern PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_capabilities)
    extern PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif

#if defined(VK_KHR_external_semaphore_fd)
    extern PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
    extern PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#endif

#if defined(VK_KHR_external_semaphore_win32)
    extern PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
    extern PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#endif

#if defined(VK_KHR_fragment_shading_rate)
    extern PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
    extern PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
#endif

#if defined(VK_KHR_get_display_properties2)
    extern PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
    extern PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
    extern PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
    extern PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
#endif

#if defined(VK_KHR_get_memory_requirements2)
    extern PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
    extern PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
    extern PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif

#if defined(VK_KHR_get_physical_device_properties2)
    extern PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
    extern PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
    extern PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
    extern PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
    extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
    extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
    extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif

#if defined(VK_KHR_get_surface_capabilities2)
    extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
    extern PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#endif

#if defined(VK_KHR_maintenance1)
    extern PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif

#if defined(VK_KHR_maintenance3)
    extern PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif

#if defined(VK_KHR_maintenance4)
    extern PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR;
    extern PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR;
    extern PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR;
#endif

#if defined(VK_KHR_performance_query)
    extern PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
    extern PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR
        vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
    extern PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR
        vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
    extern PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif

#if defined(VK_KHR_pipeline_executable_properties)
    extern PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
    extern PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
    extern PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#endif

#if defined(VK_KHR_present_wait)
    extern PFN_vkWaitForPresentKHR vkWaitForPresentKHR;
#endif

#if defined(VK_KHR_push_descriptor)
    extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#endif

#if defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline)
    extern PFN_vkCmdTraceRaysIndirect2KHR vkCmdTraceRaysIndirect2KHR;
#endif

#if defined(VK_KHR_ray_tracing_pipeline)
    extern PFN_vkCmdSetRayTracingPipelineStackSizeKHR vkCmdSetRayTracingPipelineStackSizeKHR;
    extern PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR;
    extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    extern PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
    extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    extern PFN_vkGetRayTracingShaderGroupStackSizeKHR vkGetRayTracingShaderGroupStackSizeKHR;
#endif

#if defined(VK_KHR_sampler_ycbcr_conversion)
    extern PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
    extern PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif

#if defined(VK_KHR_shared_presentable_image)
    extern PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif

#if defined(VK_KHR_surface)
    extern PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
#endif

#if defined(VK_KHR_swapchain)
    extern PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    extern PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    extern PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    extern PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    extern PFN_vkQueuePresentKHR vkQueuePresentKHR;
#endif

#if defined(VK_KHR_synchronization2)
    extern PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
    extern PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
    extern PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
    extern PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
    extern PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
    extern PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
#endif

#if defined(VK_KHR_synchronization2) && defined(VK_AMD_buffer_marker)
    extern PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
#endif

#if defined(VK_KHR_synchronization2) && defined(VK_NV_device_diagnostic_checkpoints)
    extern PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
#endif

#if defined(VK_KHR_timeline_semaphore)
    extern PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
    extern PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
    extern PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#endif

#if defined(VK_KHR_video_decode_queue)
    extern PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#endif

#if defined(VK_KHR_video_encode_queue)
    extern PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
#endif

#if defined(VK_KHR_video_queue)
    extern PFN_vkBindVideoSessionMemoryKHR vkBindVideoSessionMemoryKHR;
    extern PFN_vkCmdBeginVideoCodingKHR vkCmdBeginVideoCodingKHR;
    extern PFN_vkCmdControlVideoCodingKHR vkCmdControlVideoCodingKHR;
    extern PFN_vkCmdEndVideoCodingKHR vkCmdEndVideoCodingKHR;
    extern PFN_vkCreateVideoSessionKHR vkCreateVideoSessionKHR;
    extern PFN_vkCreateVideoSessionParametersKHR vkCreateVideoSessionParametersKHR;
    extern PFN_vkDestroyVideoSessionKHR vkDestroyVideoSessionKHR;
    extern PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParametersKHR;
    extern PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
    extern PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
    extern PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
    extern PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
#endif

#if defined(VK_KHR_wayland_surface)
    extern PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
    extern PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif

#if defined(VK_KHR_win32_surface)
    extern PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
    extern PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif

#if defined(VK_KHR_xcb_surface)
    extern PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
    extern PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

#if defined(VK_KHR_xlib_surface)
    extern PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
    extern PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif

#if defined(VK_MVK_ios_surface)
    extern PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif

#if defined(VK_MVK_macos_surface)
    extern PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif

#if defined(VK_NN_vi_surface)
    extern PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif

#if defined(VK_NVX_binary_import)
    extern PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
    extern PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
    extern PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
    extern PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
    extern PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
#endif

#if defined(VK_NVX_image_view_handle)
    extern PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
    extern PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#endif

#if defined(VK_NV_acquire_winrt_display)
    extern PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
    extern PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#endif

#if defined(VK_NV_clip_space_w_scaling)
    extern PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif

#if defined(VK_NV_cooperative_matrix)
    extern PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif

#if defined(VK_NV_copy_memory_indirect)
    extern PFN_vkCmdCopyMemoryIndirectNV vkCmdCopyMemoryIndirectNV;
    extern PFN_vkCmdCopyMemoryToImageIndirectNV vkCmdCopyMemoryToImageIndirectNV;
#endif

#if defined(VK_NV_coverage_reduction_mode)
    extern PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV
        vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#endif

#if defined(VK_NV_device_diagnostic_checkpoints)
    extern PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
    extern PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif

#if defined(VK_NV_device_generated_commands)
    extern PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
    extern PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
    extern PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
    extern PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
    extern PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
    extern PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
#endif

#if defined(VK_NV_external_memory_capabilities)
    extern PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif

#if defined(VK_NV_external_memory_rdma)
    extern PFN_vkGetMemoryRemoteAddressNV vkGetMemoryRemoteAddressNV;
#endif

#if defined(VK_NV_external_memory_win32)
    extern PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif

#if defined(VK_NV_fragment_shading_rate_enums)
    extern PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#endif

#if defined(VK_NV_memory_decompression)
    extern PFN_vkCmdDecompressMemoryIndirectCountNV vkCmdDecompressMemoryIndirectCountNV;
    extern PFN_vkCmdDecompressMemoryNV vkCmdDecompressMemoryNV;
#endif

#if defined(VK_NV_mesh_shader)
    extern PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
    extern PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
    extern PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#endif

#if defined(VK_NV_optical_flow)
    extern PFN_vkBindOpticalFlowSessionImageNV vkBindOpticalFlowSessionImageNV;
    extern PFN_vkCmdOpticalFlowExecuteNV vkCmdOpticalFlowExecuteNV;
    extern PFN_vkCreateOpticalFlowSessionNV vkCreateOpticalFlowSessionNV;
    extern PFN_vkDestroyOpticalFlowSessionNV vkDestroyOpticalFlowSessionNV;
    extern PFN_vkGetPhysicalDeviceOpticalFlowImageFormatsNV vkGetPhysicalDeviceOpticalFlowImageFormatsNV;
#endif

#if defined(VK_NV_ray_tracing)
    extern PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
    extern PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
    extern PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
    extern PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
    extern PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
    extern PFN_vkCompileDeferredNV vkCompileDeferredNV;
    extern PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
    extern PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
    extern PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
    extern PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
    extern PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
    extern PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
#endif

#if defined(VK_NV_scissor_exclusive)
    extern PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif

#if defined(VK_NV_shading_rate_image)
    extern PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
    extern PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
    extern PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#endif

#if defined(VK_QCOM_tile_properties)
    extern PFN_vkGetDynamicRenderingTilePropertiesQCOM vkGetDynamicRenderingTilePropertiesQCOM;
    extern PFN_vkGetFramebufferTilePropertiesQCOM vkGetFramebufferTilePropertiesQCOM;
#endif

#if defined(VK_QNX_screen_surface)
    extern PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
    extern PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX vkGetPhysicalDeviceScreenPresentationSupportQNX;
#endif

#if defined(VK_VALVE_descriptor_set_host_mapping)
    extern PFN_vkGetDescriptorSetHostMappingVALVE vkGetDescriptorSetHostMappingVALVE;
    extern PFN_vkGetDescriptorSetLayoutHostMappingInfoVALVE vkGetDescriptorSetLayoutHostMappingInfoVALVE;
#endif

#if (defined(VK_EXT_full_screen_exclusive) && defined(VK_KHR_device_group)) ||                                         \
    (defined(VK_EXT_full_screen_exclusive) && defined(VK_VERSION_1_1))
    extern PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif

#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) ||                                 \
    (defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)) ||                                                    \
    (defined(VK_KHR_push_descriptor) && defined(VK_KHR_descriptor_update_template))
    extern PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif

#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    extern PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
    extern PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
    extern PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#endif

#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) ||                                                     \
    (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    extern PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif

#ifdef __cplusplus
}
#endif

namespace crisp
{

VkResult loadVulkanLoaderFunctions();
VkResult loadVulkanInstanceFunctions(VkInstance instance);
VkResult loadVulkanDeviceFunctions(VkDevice device);

#define IF_CONSTEXPR_VK_DESTROY_FUNC(HandleType)                                                                       \
    if constexpr (std::is_same_v<T, Vk##HandleType>)                                                                   \
    {                                                                                                                  \
        return vkDestroy##HandleType;                                                                                  \
    }                                                                                                                  \
    else

template <typename T>
inline auto getDestroyFunc()
{
    using VulkanDestroyFunc = void (*)(VkDevice, T, const VkAllocationCallbacks*);

    IF_CONSTEXPR_VK_DESTROY_FUNC(Buffer)
    IF_CONSTEXPR_VK_DESTROY_FUNC(CommandPool)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Framebuffer)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Image)
    IF_CONSTEXPR_VK_DESTROY_FUNC(ImageView)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Pipeline)
    IF_CONSTEXPR_VK_DESTROY_FUNC(PipelineLayout)
    IF_CONSTEXPR_VK_DESTROY_FUNC(RenderPass)
    IF_CONSTEXPR_VK_DESTROY_FUNC(Sampler)
    IF_CONSTEXPR_VK_DESTROY_FUNC(SwapchainKHR)
    IF_CONSTEXPR_VK_DESTROY_FUNC(AccelerationStructureKHR)

    return static_cast<VulkanDestroyFunc>(nullptr);
}

} // namespace crisp