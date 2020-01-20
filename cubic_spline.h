#ifndef _cubic_spline_h
#define _cubic_spline_h

#include "arm_math.h"
#include "math.h"

class cubic_spline
{
private:
    struct spline_tuple
    {
        double a, b, c, d, x;
    };

    spline_tuple *splines; // Сплайн
    size_t n; // Количество узлов сетки

    void free_mem(void); // Освобождение памяти

public:
    cubic_spline(); //конструктор
    ~cubic_spline(); //деструктор

    // Построение сплайна
    // x - узлы сетки, должны быть упорядочены по возрастанию, кратные узлы запрещены
    // y - значения функции в узлах сетки
    // n - количество узлов сетки
    void build_spline(const float32_t *x, const float32_t *y, size_t n);

    // Вычисление значения интерполированной функции в произвольной точке
    float32_t f(float32_t x) const;

    float32_t invf(float32_t y, float32_t a, float32_t b) const;
};

#endif