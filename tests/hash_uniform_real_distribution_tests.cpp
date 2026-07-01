#include "terrain/utils/hash_random.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>


namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void testBounds() {
    wgen::HashUniformRealDistribution<float> dist{-2.0f, 5.0f};

    for (std::uint64_t i = 0; i < 1'000'000; ++i) {
        const std::uint64_t h = wgen::splitmix64(i);
        const float v = dist(h);

        require(v >= -2.0f, "value below min");
        require(v < 5.0f, "value greater than or equal to max");
    }
}

void testDeterminism() {
    wgen::HashUniformRealDistribution<float> dist{0.0f, 1.0f};

    const std::uint64_t h = 0x123456789ABCDEFull;

    const float a = dist(h);
    const float b = dist(h);

    require(a == b, "same hash produced different values");
}

void testMeanAndVariance() {
    wgen::HashUniformRealDistribution<float> dist{0.0f, 1.0f};

    constexpr std::size_t n = 1'000'000;

    double sum = 0.0;
    double sumSquares = 0.0;

    for (std::uint64_t i = 0; i < n; ++i) {
        const double v = dist(wgen::splitmix64(i));

        sum += v;
        sumSquares += v * v;
    }

    const double mean = sum / static_cast<double>(n);
    const double variance = sumSquares / static_cast<double>(n) - mean * mean;

    constexpr double expectedMean = 0.5;
    constexpr double expectedVariance = 1.0 / 12.0;

    require(std::abs(mean - expectedMean) < 0.001,
            "mean is too far from 0.5: " + std::to_string(mean));

    require(std::abs(variance - expectedVariance) < 0.001,
            "variance is too far from 1/12: " + std::to_string(variance));
}

void testHistogramChiSquare() {
    wgen::HashUniformRealDistribution<float> dist{0.0f, 1.0f};

    constexpr std::size_t n = 1'000'000;
    constexpr std::size_t bucketCount = 100;

    std::vector<std::size_t> buckets(bucketCount, 0);

    for (std::uint64_t i = 0; i < n; ++i) {
        const float v = dist(wgen::splitmix64(i));

        std::size_t bucket = static_cast<std::size_t>(v * bucketCount);

        if (bucket >= bucketCount) {
            bucket = bucketCount - 1;
        }

        ++buckets[bucket];
    }

    const double expected = static_cast<double>(n) / static_cast<double>(bucketCount);

    double chiSquare = 0.0;

    for (std::size_t count : buckets) {
        const double diff = static_cast<double>(count) - expected;
        chiSquare += diff * diff / expected;
    }

    // Degrees of freedom = bucketCount - 1 = 99.
    // Expected chi-square is about 99.
    // This threshold is deliberately loose to avoid overfitting the test.
    require(chiSquare < 160.0,
            "chi-square statistic too large: " + std::to_string(chiSquare));
}

void testNeighborCorrelation() {
    wgen::HashUniformRealDistribution<float> dist{0.0f, 1.0f};

    constexpr std::size_t n = 1'000'000;

    double sumA = 0.0;
    double sumB = 0.0;
    double sumAA = 0.0;
    double sumBB = 0.0;
    double sumAB = 0.0;

    for (std::uint64_t i = 0; i < n; ++i) {
        const double a = dist(wgen::splitmix64(i));
        const double b = dist(wgen::splitmix64(i + 1));

        sumA += a;
        sumB += b;
        sumAA += a * a;
        sumBB += b * b;
        sumAB += a * b;
    }

    const double invN = 1.0 / static_cast<double>(n);

    const double meanA = sumA * invN;
    const double meanB = sumB * invN;

    const double varA = sumAA * invN - meanA * meanA;
    const double varB = sumBB * invN - meanB * meanB;

    const double covariance = sumAB * invN - meanA * meanB;
    const double correlation = covariance / std::sqrt(varA * varB);

    require(std::abs(correlation) < 0.005,
            "neighbor correlation too large: " + std::to_string(correlation));
}

} // namespace


int main() {
    testBounds();
    testDeterminism();
    testMeanAndVariance();
    testHistogramChiSquare();
    testNeighborCorrelation();
}
