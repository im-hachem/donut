![Logo](branding/Logo-Black.jpg)
A real-time black hole ray tracer.

## Table of Contents

1. [Overview](#overview)
2. [Mathematical Theory](docs/mathematical-theory.md)
   - [Einstein's Field Equations](docs/mathematical-theory.md#einsteins-field-equations)
   - [Schwarzschild Metric](docs/mathematical-theory.md#schwarzschild-metric)
   - [Geodesics: Paths of Free-Falling Particles and Light](docs/mathematical-theory.md#geodesics-paths-of-free-falling-particles-and-light)
   - [Conserved Quantities](docs/mathematical-theory.md#conserved-quantities)
3. [Ray Tracing Implementation](docs/ray-tracing-implementation.md)
   - [Ray Initialization](docs/ray-tracing-implementation.md#1-ray-initialization)
   - [Geodesic Integration](docs/ray-tracing-implementation.md#2-geodesic-integration)
   - [Intersection Testing](docs/ray-tracing-implementation.md#3-intersection-testing)
   - [Rendering Pipeline](docs/ray-tracing-implementation.md#4-rendering)
4. [Numerical Methods](docs/numerical-methods.md)
   - [Runge-Kutta 4 Integration](docs/numerical-methods.md#runge-kutta-4-rk4-integration--explained)
   - [Adaptive Step Size](docs/numerical-methods.md#adaptive-step-size)
   - [Performance Optimizations](docs/numerical-methods.md#performance-optimizations)
5. [Configuration](docs/configuration.md)
   - [Configuration Files](docs/configuration.md#configuration-files)
   - [Simulation Parameters](docs/configuration.md#simulation-parameters)
   - [Performance Settings](docs/configuration.md#performance-settings)

## Overview

Donut is a real-time black hole ray tracer that simulates the visual effects of gravitational lensing around Sagittarius A* (Sgr A*), the supermassive black hole at the center of our galaxy. The application renders images of how light would bend and distort as it passes near the black hole's intense gravitational field.

## Screenshots

![Black Hole with Accretion Disk](Branding/Screenshot1.png)
![Black hole with HDRI](Branding/Screenshot2.png)

*Real-time rendering of Sagittarius A* with volumetric accretion disk, featuring:*

- **Gravitational lensing** effects around the black hole
- **Volumetric cloud-like accretion disk** with realistic density variations
- **Dynamic rotation** with Keplerian orbital motion
- **Emission-based glow** with atmospheric scattering
- **High-quality 3D noise** for natural cloud patterns

## Key Features

- **Real-time Rendering**: GPU-accelerated compute shader implementation
- **Physically Accurate**: Based on General Relativity equations
- **Interactive Camera**: Orbital camera system with realistic constraints
- **Adaptive Performance**: Dynamic step size adjustment for optimal performance
- **Multiple Objects**: Support for rendering stars and other celestial bodies
- **Configurable Parameters**: Extensive settings for simulation and graphics
- **World Builder**: Interactive 3D scene editor with reference grid for spatial orientation

## System Requirements

- **GPU**: OpenGL 4.3+ compatible graphics card
- **Memory**: 4GB RAM minimum, 8GB recommended
- **Storage**: 100MB free space
- **OS**: Windows 10/11, Linux, macOS

For detailed information on each component, please refer to the specific documentation files in the `docs/` folder.
