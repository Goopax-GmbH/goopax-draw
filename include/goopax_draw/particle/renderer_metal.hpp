#pragma once

#include <goopax_draw/window_metal.h>

struct particle_renderer
{
    sdl_window_metal& window;
    id<MTLDevice> device;
    id<MTLFunction> vertexProgram;
    id<MTLFunction> fragmentProgram;

    void render(const goopax::buffer<Eigen::Vector3<float>>& x)
    {
        @autoreleasepool
        {
            id<CAMetalDrawable> drawable = [window.swapchain nextDrawable];

            MTLRenderPassDescriptor* renderPassDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            renderPassDesc.colorAttachments[0].texture = drawable.texture;
            renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
            renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
            renderPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.1, 1.0);

            id<MTLCommandBuffer> commandBuffer = [window.queue commandBuffer];

            MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
            pipelineDescriptor.vertexFunction = vertexProgram;
            pipelineDescriptor.fragmentFunction = fragmentProgram;
            pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

            NSError* errors;
            id<MTLRenderPipelineState> pipelineState =
                [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&errors];
            assert(pipelineState && !errors);

            id<MTLRenderCommandEncoder> renderEncoder =
                [commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
            [renderEncoder setRenderPipelineState:pipelineState];
            [renderEncoder setVertexBuffer:get_metal_mem(x) offset:0 atIndex:0];
            [renderEncoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:x.size() instanceCount:1];
            [renderEncoder endEncoding];
            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }
    }

    particle_renderer(sdl_window_metal& window0)
        : window(window0)
    {
        device = static_cast<id<MTLDevice>>(window.device.get_device_ptr());

        NSString* progSrc = @"\
	struct VertexOut {\
	float4 position [[position]];\
	float pointSize "
                            @"[[point_size]];\
	};\
	\
	fragment half4 basic_fragment() {\
	return "
                            @"half4(1.0);\
	}\
	vertex VertexOut particle_vertex(const device packed_float3* "
                            @"vertex_array [[buffer(0)]],\
	unsigned int vid [[vertex_id]]) {\
	VertexOut "
                            @"vertexOut;\
	float3 position = vertex_array[vid];\
	vertexOut.position = "
                            @"float4(position.x, position.y, 0, 1);\
	vertexOut.pointSize = 1;\
	return "
                            @"vertexOut;\
	}\
	";

        NSError* errors;

        (void)progSrc;
        id<MTLLibrary> library = [device newLibraryWithSource:progSrc options:nil error:&errors];
        if (errors != nullptr)
        {
            NSLog(@"%@", errors);
        }
        assert(!errors);
        vertexProgram = [library newFunctionWithName:@"particle_vertex"];
        fragmentProgram = [library newFunctionWithName:@"basic_fragment"];
    }
};
