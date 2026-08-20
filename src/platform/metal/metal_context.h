#pragma once

// Pure-C++ interface to the Metal backend (no Objective-C leaks into the rest
// of the engine). Implemented in MetalContext.mm.
namespace Donut
{
    // Phase 1 bring-up: creates the default Metal device + command queue and
    // logs its capabilities, proving the Metal toolchain and build integration
    // work natively on Apple Silicon. This grows into the Metal compute island
    // that runs the geodesic ray tracer in real time.
    auto metal_probe() -> bool;

    // Compiles + dispatches a trivial compute kernel and validates the read-back,
    // proving the Metal compute path the geodesic ray tracer will run on.
    auto metal_compute_self_test() -> bool;
}
