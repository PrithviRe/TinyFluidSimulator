#include "FluidGrid.hpp"

#include <algorithm>
#include <cmath>

FluidGrid::FluidGrid(int n, float diffusion, float viscosity)
    : N(n), size((n + 2) * (n + 2)), diff(diffusion), visc(viscosity),
      u(size, 0.0f), v(size, 0.0f), u_prev(size, 0.0f), v_prev(size, 0.0f),
      r(size, 0.0f), g(size, 0.0f), b(size, 0.0f),
      r_prev(size, 0.0f), g_prev(size, 0.0f), b_prev(size, 0.0f) {}

void FluidGrid::reset() {
    std::fill(u.begin(), u.end(), 0.0f);
    std::fill(v.begin(), v.end(), 0.0f);
    std::fill(u_prev.begin(), u_prev.end(), 0.0f);
    std::fill(v_prev.begin(), v_prev.end(), 0.0f);
    std::fill(r.begin(), r.end(), 0.0f);
    std::fill(g.begin(), g.end(), 0.0f);
    std::fill(b.begin(), b.end(), 0.0f);
    std::fill(r_prev.begin(), r_prev.end(), 0.0f);
    std::fill(g_prev.begin(), g_prev.end(), 0.0f);
    std::fill(b_prev.begin(), b_prev.end(), 0.0f);
}

void FluidGrid::addDensity(int x, int y, float red, float green, float blue, int radius) {
    for (int j = -radius; j <= radius; ++j) {
        int cy = y + j;
        if (cy < 1 || cy > N) continue;

        for (int i = -radius; i <= radius; ++i) {
            int cx = x + i;
            if (cx < 1 || cx > N) continue;

            float distSq = static_cast<float>(i * i + j * j);
            float radSq = static_cast<float>(radius * radius);
            if (distSq <= radSq) {
                float falloff = 1.0f - distSq / radSq;
                int idx = IX(cx, cy);
                r[idx] = std::min(1.0f, r[idx] + red * falloff);
                g[idx] = std::min(1.0f, g[idx] + green * falloff);
                b[idx] = std::min(1.0f, b[idx] + blue * falloff);
            }
        }
    }
}

void FluidGrid::addVelocity(int x, int y, float vx, float vy, int radius) {
    for (int j = -radius; j <= radius; ++j) {
        int cy = y + j;
        if (cy < 1 || cy > N) continue;

        for (int i = -radius; i <= radius; ++i) {
            int cx = x + i;
            if (cx < 1 || cx > N) continue;

            float distSq = static_cast<float>(i * i + j * j);
            float radSq = static_cast<float>(radius * radius);
            if (distSq <= radSq) {
                float falloff = 1.0f - distSq / radSq;
                int idx = IX(cx, cy);
                u[idx] += vx * falloff;
                v[idx] += vy * falloff;
            }
        }
    }
}

void FluidGrid::setBnd(int bnd, std::vector<float>& x) {
    for (int i = 1; i <= N; ++i) {
        x[IX(0, i)]     = (bnd == 1) ? -x[IX(1, i)]     : x[IX(1, i)];
        x[IX(N + 1, i)] = (bnd == 1) ? -x[IX(N, i)]     : x[IX(N, i)];
        x[IX(i, 0)]     = (bnd == 2) ? -x[IX(i, 1)]     : x[IX(i, 1)];
        x[IX(i, N + 1)] = (bnd == 2) ? -x[IX(i, N)]     : x[IX(i, N)];
    }

    x[IX(0, 0)]         = 0.5f * (x[IX(1, 0)] + x[IX(0, 1)]);
    x[IX(0, N + 1)]     = 0.5f * (x[IX(1, N + 1)] + x[IX(0, N)]);
    x[IX(N + 1, 0)]     = 0.5f * (x[IX(N, 0)] + x[IX(N + 1, 1)]);
    x[IX(N + 1, N + 1)] = 0.5f * (x[IX(N, N + 1)] + x[IX(N + 1, N)]);
}

void FluidGrid::linSolve(int bnd, std::vector<float>& x, const std::vector<float>& x0, float a, float c) {
    float cRecip = 1.0f / c;
    for (int iter = 0; iter < 20; ++iter) {
        for (int j = 1; j <= N; ++j) {
            for (int i = 1; i <= N; ++i) {
                x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i - 1, j)] + x[IX(i + 1, j)] +
                                                   x[IX(i, j - 1)] + x[IX(i, j + 1)])) * cRecip;
            }
        }
        setBnd(bnd, x);
    }
}

void FluidGrid::diffuse(int bnd, std::vector<float>& x, const std::vector<float>& x0, float diffRate, float dt) {
    float a = dt * diffRate * static_cast<float>(N * N);
    linSolve(bnd, x, x0, a, 1.0f + 4.0f * a);
}

void FluidGrid::project(std::vector<float>& uField, std::vector<float>& vField,
                        std::vector<float>& p, std::vector<float>& div) {
    float h = 1.0f / static_cast<float>(N);

    for (int j = 1; j <= N; ++j) {
        for (int i = 1; i <= N; ++i) {
            div[IX(i, j)] = -0.5f * h * (uField[IX(i + 1, j)] - uField[IX(i - 1, j)] +
                                         vField[IX(i, j + 1)] - vField[IX(i, j - 1)]);
            p[IX(i, j)] = 0.0f;
        }
    }
    setBnd(0, div);
    setBnd(0, p);

    linSolve(0, p, div, 1.0f, 4.0f);

    for (int j = 1; j <= N; ++j) {
        for (int i = 1; i <= N; ++i) {
            uField[IX(i, j)] -= 0.5f * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) * static_cast<float>(N);
            vField[IX(i, j)] -= 0.5f * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) * static_cast<float>(N);
        }
    }
    setBnd(1, uField);
    setBnd(2, vField);
}

void FluidGrid::advect(int bnd, std::vector<float>& d, const std::vector<float>& d0,
                       const std::vector<float>& uField, const std::vector<float>& vField, float dt) {
    float dt0 = dt * static_cast<float>(N);
    float nFloat = static_cast<float>(N);

    for (int j = 1; j <= N; ++j) {
        for (int i = 1; i <= N; ++i) {
            float x = static_cast<float>(i) - dt0 * uField[IX(i, j)];
            float y = static_cast<float>(j) - dt0 * vField[IX(i, j)];

            if (x < 0.5f) x = 0.5f;
            if (x > nFloat + 0.5f) x = nFloat + 0.5f;
            int i0 = static_cast<int>(x);
            int i1 = i0 + 1;

            if (y < 0.5f) y = 0.5f;
            if (y > nFloat + 0.5f) y = nFloat + 0.5f;
            int j0 = static_cast<int>(y);
            int j1 = j0 + 1;

            float s1 = x - static_cast<float>(i0);
            float s0 = 1.0f - s1;
            float t1 = y - static_cast<float>(j0);
            float t0 = 1.0f - t1;

            d[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)]) +
                          s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
        }
    }
    setBnd(bnd, d);
}

void FluidGrid::step(float dt) {
    // Velocity solve
    diffuse(1, u_prev, u, visc, dt);
    diffuse(2, v_prev, v, visc, dt);

    project(u_prev, v_prev, u, v);

    advect(1, u, u_prev, u_prev, v_prev, dt);
    advect(2, v, v_prev, u_prev, v_prev, dt);

    project(u, v, u_prev, v_prev);

    // Dye / Density solve
    diffuse(0, r_prev, r, diff, dt);
    diffuse(0, g_prev, g, diff, dt);
    diffuse(0, b_prev, b, diff, dt);

    advect(0, r, r_prev, u, v, dt);
    advect(0, g, g_prev, u, v, dt);
    advect(0, b, b_prev, u, v, dt);

    // Subtle dissipation so dye gently dissolves over time
    constexpr float dissipation = 0.994f;
    for (int i = 0; i < size; ++i) {
        r[i] *= dissipation;
        g[i] *= dissipation;
        b[i] *= dissipation;
    }
}

void FluidGrid::getPixels(std::vector<uint8_t>& outPixels, bool showVelocity) const {
    outPixels.resize(N * N * 3);

    for (int j = 1; j <= N; ++j) {
        int py = j - 1;
        for (int i = 1; i <= N; ++i) {
            int px = i - 1;
            int pIndex = (py * N + px) * 3;
            int gIndex = IX(i, j);

            if (showVelocity) {
                float vx = u[gIndex] * 0.15f;
                float vy = v[gIndex] * 0.15f;
                float speed = std::sqrt(vx * vx + vy * vy);

                float redVal   = std::clamp(0.5f + vx, 0.0f, 1.0f);
                float greenVal = std::clamp(0.5f + vy, 0.0f, 1.0f);
                float blueVal  = std::clamp(speed, 0.0f, 1.0f);

                outPixels[pIndex + 0] = static_cast<uint8_t>(redVal * 255.0f);
                outPixels[pIndex + 1] = static_cast<uint8_t>(greenVal * 255.0f);
                outPixels[pIndex + 2] = static_cast<uint8_t>(blueVal * 255.0f);
            } else {
                float redVal   = std::clamp(r[gIndex], 0.0f, 1.0f);
                float greenVal = std::clamp(g[gIndex], 0.0f, 1.0f);
                float blueVal  = std::clamp(b[gIndex], 0.0f, 1.0f);

                outPixels[pIndex + 0] = static_cast<uint8_t>(redVal * 255.0f);
                outPixels[pIndex + 1] = static_cast<uint8_t>(greenVal * 255.0f);
                outPixels[pIndex + 2] = static_cast<uint8_t>(blueVal * 255.0f);
            }
        }
    }
}

