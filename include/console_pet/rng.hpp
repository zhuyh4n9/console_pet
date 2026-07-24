#pragma once

// Small random-number helper that mirrors Python's `random` module semantics:
//   rand_int(lo, hi) -> inclusive integer in [lo, hi]
//   rand_real()      -> double in [0, 1)
//
// A single shared generator is exposed via rng() so the whole program behaves
// like Python's module-level `random`, while still being seedable for tests.

#include <random>

namespace pet {

class Rng {
public:
    Rng() : gen_(std::random_device{}()) {}

    /// Inclusive integer in [lo, hi]. Requires lo <= hi.
    int rand_int(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(gen_);
    }

    /// Real in [0, 1).
    double rand_real() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(gen_);
    }

    void seed(unsigned s) { gen_.seed(s); }

private:
    std::mt19937 gen_;
};

/// Process-wide shared generator (analogue of Python's global `random`).
inline Rng& rng() {
    static Rng instance;
    return instance;
}

}  // namespace pet
