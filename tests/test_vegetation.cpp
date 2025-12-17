#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/vegetation/vegetation_types.h"
#include "../src/vegetation/vegetation_system.h"

using namespace vegetation;

void test_initialization() {
    std::cout << "Running test_initialization..." << std::endl;
    VegetationGrid grid;
    grid.resize(10, 10);
    assert(grid.ei_coverage[0] == 1.0f);
    assert(grid.es_coverage[0] == 0.0f);
    assert(grid.isValid());
    std::cout << "PASSED" << std::endl;
}

void test_disturbance() {
    std::cout << "Running test_disturbance..." << std::endl;
    VegetationGrid grid;
    grid.resize(10, 10);

    DisturbanceRegime regime;
    regime.magnitude = 0.5f;
    regime.frequency = 1.0f;
    regime.spatialExtent = 1.0f; // 100% impact

    VegetationSystem::applyDisturbance(grid, regime);

    // Initial 1.0 - 0.5 = 0.5
    assert(std::abs(grid.ei_coverage[0] - 0.5f) < 0.01f);
    assert(grid.recovery_timer[0] > 0.0f); // Memory set
    std::cout << "PASSED" << std::endl;
}

void test_recovery() {
    std::cout << "Running test_recovery..." << std::endl;
    VegetationGrid grid;
    grid.resize(1, 1);
    
    // Simulate damage
    grid.ei_coverage[0] = 0.1f;
    grid.recovery_timer[0] = 0.1f; // Short memory

    // Update 1: Timer decrement
    VegetationSystem::update(grid, 0.1f);
    assert(grid.recovery_timer[0] <= 0.001f);
    assert(grid.ei_coverage[0] == 0.1f); // No growth yet

    // Update 2: Growth
    VegetationSystem::update(grid, 1.0f);
    assert(grid.ei_coverage[0] > 0.1f); // Should grow
    std::cout << "PASSED: EI grew to " << grid.ei_coverage[0] << std::endl;
}

void test_invariant() {
    std::cout << "Running test_invariant..." << std::endl;
    VegetationGrid grid;
    grid.resize(1, 1);
    
    // Force invalid state
    grid.ei_coverage[0] = 0.8f;
    grid.es_coverage[0] = 0.8f;

    VegetationSystem::update(grid, 0.0f); // Just enforcing

    float sum = grid.ei_coverage[0] + grid.es_coverage[0];
    assert(sum <= 1.0001f);
    assert(std::abs(grid.ei_coverage[0] - 0.5f) < 0.01f); // 0.8 / 1.6 = 0.5
    std::cout << "PASSED" << std::endl;
}

int main() {
    test_initialization();
    test_disturbance();
    test_recovery();
    test_invariant();
    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
