/*
 * Mandrake - the screen's image for a picture drawn on the CPU (Beetle PSX), included by vk_present.c.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Included where V (vk_present.c's state), FRAMES, allocate(), barrier(), SwanStationPS5_log and the Vulkan functions
 * are known. tests/core/beetle_headless.c includes it too, with its own device (lavapipe on the CI's runner), and
 * uploads Beetle PSX's pictures through it: what it reads back must be the picture, pixel for pixel.
 */

/* The picture of a core that draws on the CPU (Beetle PSX; SwanStation with its CPU renderer): uploaded into an image of
 * the screen's own, which then stands in for a GPU core's image (V.game_image / game_view). Everything that reads the
 * game picture (the shaders and their chains, FSR 1, CAS, frame generation, supersampling) takes it as it takes
 * SwanStation's. Made on the device the screen is on, again when the picture changes size; its staging buffers, one
 * per frame in flight, are written only once that frame's fence has passed. */
static struct
{
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t w, h;
    bool written; /* it has had a picture: its layout is SHADER_READ_ONLY_OPTIMAL */
    VkBuffer staging[FRAMES];
    VkDeviceMemory staging_memory[FRAMES];
    void *staging_map[FRAMES];
    VkDeviceSize staging_size[FRAMES];
    const uint32_t *pixels; /* the picture to upload with the next frame (cpu_frame.h's memory), w x h packed */
    bool pending;
    int warned;
} C;

static void cpu_image_free(bool staging_too)
{
    if (C.view)
        vkDestroyImageView(V.device, C.view, NULL);
    if (C.image)
        vkDestroyImage(V.device, C.image, NULL);
    if (C.memory)
        vkFreeMemory(V.device, C.memory, NULL);
    C.view = VK_NULL_HANDLE;
    C.image = VK_NULL_HANDLE;
    C.memory = VK_NULL_HANDLE;
    C.w = C.h = 0;
    C.written = false;
    for (int f = 0; f < FRAMES && staging_too; ++f)
    {
        if (C.staging[f])
            vkDestroyBuffer(V.device, C.staging[f], NULL);
        if (C.staging_memory[f])
            vkFreeMemory(V.device, C.staging_memory[f], NULL);
        C.staging[f] = VK_NULL_HANDLE;
        C.staging_memory[f] = VK_NULL_HANDLE;
        C.staging_map[f] = NULL;
        C.staging_size[f] = 0;
    }
}

/* The image for a w x h picture: XRGB8888 in memory is B, G, R, X bytes, which B8G8R8A8 reads as they are; the view
 * reads its alpha as 1 (the X byte is not transparency). */
static bool cpu_image_ensure(uint32_t w, uint32_t h)
{
    if (C.image && C.w == w && C.h == h)
        return true;
    if (C.image)
    {
        vkDeviceWaitIdle(V.device); /* the frames in flight may still read the old one */
        cpu_image_free(false);
    }
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_B8G8R8A8_UNORM;
    ii.extent = (VkExtent3D){w, h, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkMemoryRequirements req;
    if (vkCreateImage(V.device, &ii, NULL, &C.image) != VK_SUCCESS)
    {
        C.image = VK_NULL_HANDLE;
        return false;
    }
    vkGetImageMemoryRequirements(V.device, C.image, &req);
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = C.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = ii.format;
    view.components.a = VK_COMPONENT_SWIZZLE_ONE;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    if (!allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &C.memory) ||
        vkBindImageMemory(V.device, C.image, C.memory, 0) != VK_SUCCESS ||
        vkCreateImageView(V.device, &view, NULL, &C.view) != VK_SUCCESS)
    {
        C.view = VK_NULL_HANDLE;
        cpu_image_free(false);
        return false;
    }
    C.w = w;
    C.h = h;
    C.written = false;
    return true;
}

/* This frame's copy of the picture, in its staging buffer (the frame's fence has passed: nothing reads it). */
static bool cpu_stage(int f)
{
    const VkDeviceSize size = (VkDeviceSize)C.w * C.h * 4;
    if (C.staging_size[f] < size)
    {
        if (C.staging[f])
            vkDestroyBuffer(V.device, C.staging[f], NULL);
        if (C.staging_memory[f])
            vkFreeMemory(V.device, C.staging_memory[f], NULL);
        C.staging[f] = VK_NULL_HANDLE;
        C.staging_memory[f] = VK_NULL_HANDLE;
        C.staging_map[f] = NULL;
        C.staging_size[f] = 0;
        VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = size;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryRequirements req;
        if (vkCreateBuffer(V.device, &bi, NULL, &C.staging[f]) != VK_SUCCESS)
        {
            C.staging[f] = VK_NULL_HANDLE;
            return false;
        }
        vkGetBufferMemoryRequirements(V.device, C.staging[f], &req);
        if (!allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &C.staging_memory[f]) ||
            vkBindBufferMemory(V.device, C.staging[f], C.staging_memory[f], 0) != VK_SUCCESS ||
            vkMapMemory(V.device, C.staging_memory[f], 0, VK_WHOLE_SIZE, 0, &C.staging_map[f]) != VK_SUCCESS)
        {
            vkDestroyBuffer(V.device, C.staging[f], NULL);
            if (C.staging_memory[f])
                vkFreeMemory(V.device, C.staging_memory[f], NULL);
            C.staging[f] = VK_NULL_HANDLE;
            C.staging_memory[f] = VK_NULL_HANDLE;
            C.staging_map[f] = NULL;
            return false;
        }
        C.staging_size[f] = size;
    }
    memcpy(C.staging_map[f], C.pixels, (size_t)size);
    return true;
}

/* The picture into the image, at the start of this frame's commands: everything after reads it as the core's. The
 * barrier before also waits for the frames still reading the last picture (one queue: they were submitted before). */
static void cpu_upload(VkCommandBuffer cb, int f)
{
    if (!cpu_stage(f))
    {
        if (!C.warned++)
            SwanStationPS5_log("vulkan: no memory to upload the %ux%u picture", C.w, C.h);
        return;
    }
    barrier(cb, C.image, C.written ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy copy = {0};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = (VkExtent3D){C.w, C.h, 1};
    vkCmdCopyBufferToImage(cb, C.staging[f], C.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier(cb, C.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    C.written = true;
}

bool vkp_set_game_pixels(const uint32_t *pixels, int w, int h)
{
    if (!V.device || !V.swapchain || !pixels || w <= 0 || h <= 0)
        return false;
    if (!cpu_image_ensure((uint32_t)w, (uint32_t)h))
    {
        if (!C.warned++)
            SwanStationPS5_log("vulkan: no image for the %dx%d picture: it is drawn by the CPU", w, h);
        return false;
    }
    C.pixels = pixels;
    C.pending = true;
    V.game_image = C.image;
    V.game_view = C.view;
    V.game_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return true;
}

void vkp_count_frame(bool repeated)
{
    ++V.core_frames;
    if (!repeated)
        ++V.new_frames;
}
