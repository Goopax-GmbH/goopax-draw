#include "../window_vulkan.h"

namespace goopax_draw::vulkan
{
struct Semaphore
{
    sdl_window_vulkan& window;
    VkSemaphore vkSemaphore;

    Semaphore(sdl_window_vulkan& window);
    ~Semaphore();
};
}
