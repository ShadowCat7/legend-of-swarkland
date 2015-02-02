#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "util.hpp"

struct Coord {
    int x;
    int y;
    Coord() :
            x(0), y(0) {
    }
    Coord(int x, int y) :
            x(x), y(y) {
    }
};
static inline bool operator==(Coord a, Coord b) {
    return a.x == b.x && a.y == b.y;
}
static inline bool operator!=(Coord a, Coord b) {
    return !(a == b);
}
static inline Coord operator+(Coord a, Coord b) {
    return Coord(b.x + a.x, b.y + a.y);
}
static inline Coord operator-(Coord a, Coord b) {
    return Coord(a.x - b.x, a.y - b.y);
}

template<typename T>
class Matrix {
public:
    Matrix(int height, int width) :
            _height(height), _width(width) {
        items = new T[height * width];
    }
    Matrix(Matrix &) = delete;
    ~Matrix() {
        delete[] items;
    }
    T & operator[](Coord index) {
        if (index.x < 0 || index.x >= _width || index.y < 0 || index.y >= _height)
            panic("matrix bounds check");
        return items[index.y * _width + index.x];
    }
    void set_all(T value) {
        for (int i = 0; i < _width * _height; i++) {
            items[i] = value;
        }
    }
private:
    T * items;
    int _height;
    int _width;
};

static inline int distance_squared(Coord a, Coord b) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    return dx * dx + dy * dy;
}

// each of x and y will be -1, 0, 1
static inline Coord sign(Coord value) {
    return Coord(sign(value.x), sign(value.y));
}

#endif
