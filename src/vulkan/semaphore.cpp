#include <goopax_draw/vulkan/semaphore.hpp>

namespace goopax_draw::vulkan
{
Semaphore::Semaphore(sdl_window_vulkan& window0)
    : window(window0)
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    call_vulkan(window.vkCreateSemaphore(window.vkDevice, &semaphoreInfo, nullptr, &vkSemaphore));
}

Semaphore::~Semaphore()
{
    window.vkDestroySemaphore(window.vkDevice, vkSemaphore, nullptr);
}
}
