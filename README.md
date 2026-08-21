![Logo](branding/Logo-Monochrome-White.jpg)

Donut is a real-time renderer for Sagittarius A* (Sgr A*) that traces light through the curved spacetime surrounding the black hole. Each pixel is represented by a light ray originating from the camera. The ray is integrated through the Schwarzschild metric, allowing the renderer to reproduce gravitational lensing and the distortion of the surrounding accretion disk and background. The renderer runs on the GPU using compute shaders, allowing the scene to be explored interactively in real time.

## Screenshots
![Black Hole with Accretion Disk](branding/Screenshot1.png)
![Black Hole with HDRI](branding/Screenshot2.png)

## Physics
Donut uses the Schwarzschild solution to describe the spacetime around Sgr A*. The metric is (with $r_s = \frac{2GM}{c^2}$):

$$
ds^2 =
-\left(1-\frac{r_s}{r}\right)dt^2
+\left(1-\frac{r_s}{r}\right)^{-1}dr^2
+r^2(d\theta^2+\sin^2\theta,d\phi^2),
$$

Light rays are propagated along null geodesics of this metric. The resulting equations are integrated numerically using fourth-order Runge-Kutta (RK4), with the ray represented in spherical coordinates along with its derivatives and conserved quantities. The Schwarzschild metric provides conserved energy and angular momentum along each geodesic. These quantities are used during integration rather than explicitly evolving every coordinate of the ray.

Each ray is integrated step by step until it reaches the event horizon, intersects an object or the accretion disk, or escapes the region being rendered.
The integration uses adaptive step sizes. Rays passing through regions of strong curvature require smaller steps, while rays far from the black hole can be advanced more quickly. Rays that are clearly escaping are terminated early to avoid unnecessary computation. RK4 was chosen to provide sufficient accuracy for the rapidly changing trajectories near the black hole while still being practical to run for large numbers of rays on the GPU.
The curvature of spacetime changes the direction of each ray as it passes around the black hole. Rays passing close to the photon sphere can undergo large deflections or orbit the black hole several times before escaping. This produces the distorted background, multiple images of the accretion disk, and the bright lensing structures surrounding the shadow.

## Rendering
For each pixel, Donut generates a ray from the camera using its position, orientation, field of view, and aspect ratio. The ray is then converted into the coordinates used by the geodesic integrator and propagated through the scene. During integration, the renderer checks for intersections with the black hole, accretion disk, and other scene objects. Rays that escape are sampled against the background environment. The accretion disk is represented as a volumetric medium rather than a flat textured surface. Its density is generated procedurally using 3D noise, giving it a cloud-like structure, and the disk rotates according to Keplerian orbital motion. Since the disk is viewed through curved spacetime, different parts of it can reach the camera along multiple paths around the black hole. The resulting image includes gravitational lensing, gravitational redshift, and Doppler shifting from the rotating disk. The final pixel color is determined from the ray's path, its intersection with the scene, and the relativistic effects accumulated along the way. The entire process is performed on the GPU using compute shaders, allowing thousands of rays to be integrated simultaneously.

The geodesic equations are integrated using fourth-order Runge-Kutta (RK4):

$$
y_n+
\frac{h}{6}
(k_1+2k_2+2k_3+k_4).
$$

RK4 has a local truncation error of $O(h^5)$ and a global error of $O(h^4)$, making it suitable for maintaining accurate trajectories without requiring extremely small steps everywhere. Adaptive stepping and early termination are used alongside RK4 to keep the cost of tracing rays manageable. Rays near the black hole receive more computation than rays that are already far from the gravitational field.
\

## License
Donut is released under the MIT License.
The source code may be used, modified, and redistributed freely, including in commercial projects, provided that the original copyright notice and license are retained.
