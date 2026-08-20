#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "metal_context.h"
#include "core/log.h"

#include <vector>

namespace Donut
{
    bool metal_probe()
    {
        @autoreleasepool
        {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            if (device == nil)
            {
                DONUT_ERROR("Metal: no default device available");
                return false;
            }

            id<MTLCommandQueue> queue = [device newCommandQueue];
            const char* name = [[device name] UTF8String];

            DONUT_INFO("Metal device: {}", name ? name : "(unknown)");
            DONUT_INFO("Metal: unified memory = {}, max threads/threadgroup = {}",
                       device.hasUnifiedMemory ? "yes" : "no",
                       (unsigned long)device.maxThreadsPerThreadgroup.width);

            if (queue == nil)
            {
                DONUT_WARN("Metal: failed to create command queue");
                return false;
            }

            return true;
        }
    }

    // Compiles a trivial compute kernel, dispatches it over a small texture, and
    // reads the result back to confirm the full Metal compute path works: source
    // compilation, pipeline state, command encoding, dispatch, and shared-memory
    // read-back. This is the mechanism the geodesic ray tracer will run on.
    bool metal_compute_self_test()
    {
        @autoreleasepool
        {
            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
            id<MTLCommandQueue> queue = [device newCommandQueue];
            if (device == nil || queue == nil)
                return false;

            NSString* src =
                @"#include <metal_stdlib>\n"
                 "using namespace metal;\n"
                 "kernel void selfTest(texture2d<float, access::write> outTex [[texture(0)]],\n"
                 "                     uint2 gid [[thread_position_in_grid]])\n"
                 "{\n"
                 "    uint w = outTex.get_width();\n"
                 "    uint h = outTex.get_height();\n"
                 "    if (gid.x >= w || gid.y >= h) return;\n"
                 "    outTex.write(float4(float(gid.x) / float(w - 1),\n"
                 "                        float(gid.y) / float(h - 1), 0.5, 1.0), gid);\n"
                 "}\n";

            NSError* err = nil;
            id<MTLLibrary> lib = [device newLibraryWithSource:src options:nil error:&err];
            if (lib == nil)
            {
                DONUT_ERROR("Metal self-test: kernel compile failed: {}",
                            err ? [[err localizedDescription] UTF8String] : "unknown");
                return false;
            }

            id<MTLFunction> fn = [lib newFunctionWithName:@"selfTest"];
            id<MTLComputePipelineState> pipeline =
                [device newComputePipelineStateWithFunction:fn error:&err];
            if (pipeline == nil)
            {
                DONUT_ERROR("Metal self-test: pipeline creation failed");
                return false;
            }

            const uint32_t W = 64, H = 64;
            MTLTextureDescriptor* desc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                   width:W
                                                                  height:H
                                                               mipmapped:NO];
            desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
            desc.storageMode = MTLStorageModeShared;
            id<MTLTexture> tex = [device newTextureWithDescriptor:desc];

            id<MTLCommandBuffer> cb = [queue commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
            [enc setComputePipelineState:pipeline];
            [enc setTexture:tex atIndex:0];

            MTLSize tg = MTLSizeMake(16, 16, 1);
            MTLSize grid = MTLSizeMake(W, H, 1);
            [enc dispatchThreads:grid threadsPerThreadgroup:tg];
            [enc endEncoding];
            [cb commit];
            [cb waitUntilCompleted];

            // Read back the far corner; the kernel writes (~1, ~1, 0.5, 1) there.
            std::vector<uint8_t> px(W * H * 4);
            [tex getBytes:px.data()
              bytesPerRow:W * 4
               fromRegion:MTLRegionMake2D(0, 0, W, H)
              mipmapLevel:0];

            size_t corner = ((size_t)(H - 1) * W + (W - 1)) * 4;
            DONUT_INFO("Metal compute self-test: corner pixel RGBA = ({}, {}, {}, {})",
                       (int)px[corner + 0], (int)px[corner + 1],
                       (int)px[corner + 2], (int)px[corner + 3]);

            bool ok = px[corner + 0] > 250 && px[corner + 1] > 250 &&
                      px[corner + 3] == 255;
            DONUT_INFO("Metal compute self-test: {}", ok ? "PASS" : "FAIL");
            return ok;
        }
    }
}
