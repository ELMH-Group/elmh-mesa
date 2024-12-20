/*
 * Copyright © 2023 Raspberry Pi Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "v3dv_private.h"

#include "common/v3d_performance_counters.h"

VkResult
v3dX(enumerate_performance_query_counters)(struct v3dv_physical_device *pDevice,
                                           uint32_t *pCounterCount,
                                           VkPerformanceCounterKHR *pCounters,
                                           VkPerformanceCounterDescriptionKHR *pCounterDescriptions)
{
   struct v3d_device_info *devinfo = &pDevice->devinfo;
   uint32_t desc_count = *pCounterCount;
   uint8_t ncounters = devinfo->max_perfcnt ? devinfo->max_perfcnt
                                            : ARRAY_SIZE(v3d_performance_counters);

   VK_OUTARRAY_MAKE_TYPED(VkPerformanceCounterKHR,
                          out, pCounters, pCounterCount);
   VK_OUTARRAY_MAKE_TYPED(VkPerformanceCounterDescriptionKHR,
                          out_desc, pCounterDescriptions, &desc_count);

   for (int i = 0; i < ncounters; i++) {
      struct drm_v3d_perfmon_get_counter counter = {
         .counter = i,
      };
      const char *name, *category, *description;

      if (devinfo->max_perfcnt) {
         int ret = v3dv_ioctl(pDevice->render_fd, DRM_IOCTL_V3D_PERFMON_GET_COUNTER,
                              &counter);
         if (ret) {
            mesa_loge("Failed to get counter description for counter %d: %s\n",
                      i, strerror(errno));
         }

         name = (char *) counter.name;
         category = (char *) counter.category;
         description = (char *) counter.description;
      } else {
         /* Legacy path for kernels without support for DRM_IOCTL_V3D_PERFMON_GET_COUNTER */
         name = v3d_performance_counters[i][V3D_PERFCNT_NAME];
         category = v3d_performance_counters[i][V3D_PERFCNT_CATEGORY];
         description = v3d_performance_counters[i][V3D_PERFCNT_DESCRIPTION];
      }

      vk_outarray_append_typed(VkPerformanceCounterKHR, &out, counter) {
         counter->unit = VK_PERFORMANCE_COUNTER_UNIT_GENERIC_KHR;
         counter->scope = VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_KHR;
         counter->storage = VK_PERFORMANCE_COUNTER_STORAGE_UINT64_KHR;

         unsigned char sha1_result[20];
         _mesa_sha1_compute(name, strlen(name), sha1_result);

         memcpy(counter->uuid, sha1_result, sizeof(counter->uuid));
      }

      vk_outarray_append_typed(VkPerformanceCounterDescriptionKHR,
                               &out_desc, desc) {
         desc->flags = 0;
         snprintf(desc->name, sizeof(desc->name), "%s", name);
         snprintf(desc->category, sizeof(desc->category), "%s", category);
         snprintf(desc->description, sizeof(desc->description), "%s", description);
      }
   }

   return vk_outarray_status(&out);
}
