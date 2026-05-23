#pragma once

#if WITH_METAL
#include "particle/renderer_metal.hpp"
#endif

#if WITH_OPENGL && __has_include(<glatter/glatter.h>)
#include "particle/renderer_gl.hpp"
#endif

#if WITH_VULKAN
#include "particle/renderer_vulkan.hpp"
#endif
