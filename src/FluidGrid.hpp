#pragma once

#include <cstdint>
#include <vector>

class FluidGrid {
  public:
    FluidGrid(int n, float diffusion = 0.00001f, float viscosity = 0.00001f);

    void step(float dt);

    void addDensity(int x, int y, float r, float g, float b, int radius);
    void addVelocity(int x, int y, float vx, float vy, int radius);

    void getPixels(std::vector<uint8_t>& outPixels, bool showVelocity) const;
    void reset();

    int getSize() const { return N; }

  private:
    int N;
    int size;
    float diff;
    float visc;

    std::vector<float> u;
    std::vector<float> v;
    std::vector<float> u_prev;
    std::vector<float> v_prev;

    std::vector<float> r;
    std::vector<float> g;
    std::vector<float> b;
    std::vector<float> r_prev;
    std::vector<float> g_prev;
    std::vector<float> b_prev;

    inline int IX(int x, int y) const {
        return x + (N + 2) * y;
    }

    void setBnd(int bnd, std::vector<float>& x);
    void linSolve(int bnd, std::vector<float>& x, const std::vector<float>& x0, float a, float c);
    void diffuse(int bnd, std::vector<float>& x, const std::vector<float>& x0, float diffRate, float dt);
    void project(std::vector<float>& u, std::vector<float>& v, std::vector<float>& p, std::vector<float>& div);
    void advect(int bnd, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, float dt);
};

