#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <random>
#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <vector>
#include <cassert>

namespace wgen {

using colorFromHeightFunc = glm::vec3 (*)(float);

glm::vec3 terrainColor(float height);

glm::vec3 terrainBlackAndWhite(float height);

template<class T>
class HeightMap;

template<class T>
struct IsHeightMap : std::false_type {};

template<class T>
struct IsHeightMap<HeightMap<T>> : std::true_type {};

template<class T>
concept NotHeightMap = !IsHeightMap<std::remove_cvref_t<T>>::value;

template<class T>
class HeightMap {
public:
    HeightMap() : width_{0}, height_{0}, samples_{0} {}
    HeightMap(std::size_t width, std::size_t height) : width_{width}, height_{height}, samples_(width * height) {
        if (width < 2 || height < 2) {
            throw std::invalid_argument("height map dimensions must be at least 2x2");
        }
    }
    HeightMap(std::size_t width, std::size_t height, const T& defaultValue) : HeightMap(width, height) {
        this->clear(defaultValue);
    }
    HeightMap(std::initializer_list<std::initializer_list<T>> rows)
        : width_{rows.size() == 0 ? 0 : rows.begin()->size()},
          height_{rows.size()},
          samples_{} {
        if (width_ < 2 || height_ < 2) {
            throw std::invalid_argument("height map dimensions must be at least 2x2");
        }

        samples_.reserve(width_ * height_);
        for (const auto& row : rows) {
            if (row.size() != width_) {
                throw std::invalid_argument("height map initializer rows must have equal sizes");
            }
            samples_.insert(samples_.end(), row.begin(), row.end());
        }
    }

    HeightMap(const HeightMap& heightmap) = default;
    HeightMap(HeightMap&& heightmap) = default;

    HeightMap& operator=(const HeightMap&) = default;
    HeightMap& operator=(HeightMap&&) noexcept = default;

    // Adds values from a different heightmap starting at position (x, y)
    /*
    If the other heightmap is of size (w, h) and position at (x, y)
    Then this will add othe heightmap values to a sub heightmap with corners (x, y) and (x + w, y + h).
    This will also crop the result if it goes out of bounds
    */
    // void add_at(const HeightMap& other, std::size_t x = 0, std::size_t y = 0) {
    //     for (std::size_t j = y; j < std::min(height_, y + other.height()); ++j) {
    //         for (std::size_t i = x; i < std::min(width_, x + other.width()); ++i) {
    //             this->at(i, j) += other.at(i - x, j - y);
    //         }
    //     }
    // }
    template<typename N>
    requires requires (const N& n, T& t) {
        t += static_cast<T>(n);
    }
    void add_at(const HeightMap<N>& other, std::size_t x = 0, std::size_t y = 0) {
        for (std::size_t j = y; j < std::min(height_, y + other.height()); ++j) {
            for (std::size_t i = x; i < std::min(width_, x + other.width()); ++i) {
                this->at(i, j) += static_cast<T>(other.at(i - x, j - y));
            }
        }
    }

    // uses add_at method
    template<typename N>
    requires requires (const N& n, T& t) {
        t += static_cast<T>(n);
    }
    void operator+=(const HeightMap<N>& other) {
        this->add_at(other, 0, 0);
    }

    template<typename N>
    requires requires (const N& n, T& t) {
        t += static_cast<T>(n);
    }
    HeightMap<T> operator+(const HeightMap<N>& other) const {
        HeightMap<T> res(*this);
        res += other;
        return res;
    }

    template<typename N>
        requires NotHeightMap<N>
            && requires(T& t, const N& n) { t += n; }
    void operator+=(const N& rhs) {
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                this->at(x, y) += rhs;
            }
        }
    }
    template<typename N>
        requires NotHeightMap<N>
            && requires(const T& t, const N& n) { t + n; }
    HeightMap<T> operator+(const N& rhs) const {
        HeightMap<T> res(*this);
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                res.at(x, y) = res.at(x, y) + rhs;
            }
        }
        return res;
    }

    template<typename N>
    requires NotHeightMap<N> && requires(T& t, const N& n) { t -= n; }
    void operator-=(const N& rhs) {
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                this->at(x, y) -= rhs;
            }
        }
    }
    template<typename N>
    requires NotHeightMap<N> && requires(const T& t, const N& n) { t - n; }
    HeightMap<T> operator-(const N& rhs) {
        HeightMap<T> res(*this);
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                res.at(x, y) = res.at(x, y) - rhs;
            }
        }
        return res;
    }

    template<typename N>
    requires NotHeightMap<N> && requires(T& t, const N& n) { t *= n; }
    void operator*=(const N& rhs) {
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                this->at(x, y) *= rhs;
            }
        }
    }

    template<typename N>
    requires NotHeightMap<N> && requires(const T& t, const N& n) { t * n; }
    HeightMap<T> operator*(const N& rhs) const {
        HeightMap<T> res(*this);
        res *= rhs;
        return res;
    }

    template<typename N>
    requires NotHeightMap<N> && requires(const N& n, const T& t) { n * t; }
    friend HeightMap<T> operator*(const N& lhs, const HeightMap<T>& rhs) {
        HeightMap<T> res(rhs);
        for (std::size_t y = 0; y < res.height(); ++y) {
            for (std::size_t x = 0; x < res.width(); ++x) {
                res.at(x, y) = lhs * res.at(x, y);
            }
        }
        return res;
    }

    template<typename N>
        requires NotHeightMap<N>
            && requires(const N& n, const T& t) { n + t; }
    friend HeightMap<T> operator+(const N& lhs, const HeightMap<T>& rhs) {
        HeightMap<T> res(rhs);
        for (std::size_t y = 0; y < res.height(); ++y) {
            for (std::size_t x = 0; x < res.width(); ++x) {
                res.at(x, y) = lhs + res.at(x, y);
            }
        }
        return res;
    }
    template<typename N>
    requires NotHeightMap<N> && requires(const N& n, const T& t) { n - t; }
    friend HeightMap<T> operator-(const N& lhs, const HeightMap<T>& rhs) {
        HeightMap<T> res(rhs);
        for (std::size_t y = 0; y < res.height(); ++y) {
            for (std::size_t x = 0; x < res.width(); ++x) {
                res.at(x, y) = lhs - res.at(x, y);
            }
        }
        return res;
    }



    T &at(std::size_t x, std::size_t y) { return samples_.at(y * width_ + x); }
    template <typename Int> requires requires (const Int& elem) { static_cast<std::size_t>(elem); }
    T &at(const glm::vec<2, Int>& pos) {
        return this->at(static_cast<std::size_t>(pos.x), static_cast<std::size_t>(pos.y));
    }

    T at(std::size_t x, std::size_t y) const { return samples_.at(y * width_ + x); }
    template <typename Int> requires requires (const Int& elem) { static_cast<std::size_t>(elem); }
    T at(const glm::vec<2, Int>& pos) const {
        return this->at(static_cast<std::size_t>(pos.x), static_cast<std::size_t>(pos.y));
    }
    std::size_t width() const { return width_; }
    std::size_t height() const { return height_; }

    HeightMap& project(const T& minVal, const T& maxVal) {
        T mn = this->at(0, 0);
        T mx = this->at(0, 0);
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                mn = std::min<T>(mn, this->at(x, y));
                mx = std::max<T>(mx, this->at(x, y));
            }
        }
        auto middlePoint = (mn + mx) / 2.0F;

        auto newMiddlePoint = (minVal + maxVal) / 2.0F;
        auto scale = (maxVal - minVal) / (mx - mn);
        if (std::abs(mx - mn) < 0.000001F) {
            scale = 0;
        }
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                auto newVal = this->at(x, y) - mn;
                newVal *= scale;
                newVal += minVal;
                this->at(x, y) = newVal;
            }
        }
        return *this;
    }
    HeightMap& normalize() {
        return project(-1, 1);
    }
    HeightMap projected(const T& minVal, const T& maxVal) const {
        HeightMap newHeightmap{*this};
        newHeightmap.project(minVal, maxVal);
        return newHeightmap;
    }
    HeightMap normal() const {
        return projected(-1, 1);
    }

    void clear(const T& defaultVal) {
        for (std::size_t y = 0; y < height_; ++y) {
            for (std::size_t x = 0; x < width_; ++x) {
                this->at(x, y) = defaultVal;
            }
        }
    }

private:
    std::size_t width_;
    std::size_t height_;
    std::vector<T> samples_;
};

} // namespace wgen

namespace std {

template<typename T>
requires requires (const T& lhs, const T& rhs) {
    lhs < rhs;
}
T min(const wgen::HeightMap<T>& heightMap) {
    if (heightMap.width() == 0 || heightMap.height() == 0) {
        throw std::invalid_argument("cannot get minimum value of an empty height map");
    }

    T minValue = heightMap.at(0, 0);
    for (std::size_t y = 0; y < heightMap.height(); ++y) {
        for (std::size_t x = 0; x < heightMap.width(); ++x) {
            const T value = heightMap.at(x, y);
            if (value < minValue) {
                minValue = value;
            }
        }
    }

    return minValue;
}

template<typename T>
requires requires (const T& lhs, const T& rhs) {
    lhs < rhs;
}
T max(const wgen::HeightMap<T>& heightMap) {
    if (heightMap.width() == 0 || heightMap.height() == 0) {
        throw std::invalid_argument("cannot get maximum value of an empty height map");
    }

    T maxValue = heightMap.at(0, 0);
    for (std::size_t y = 0; y < heightMap.height(); ++y) {
        for (std::size_t x = 0; x < heightMap.width(); ++x) {
            const T value = heightMap.at(x, y);
            if (maxValue < value) {
                maxValue = value;
            }
        }
    }

    return maxValue;
}

} // namespace std
