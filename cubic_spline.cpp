#include "cubic_spline.h"

cubic_spline::cubic_spline() : splines(nullptr) { }

cubic_spline::~cubic_spline() {
    free_mem();
}

void cubic_spline::build_spline(const float32_t *x, const float32_t *y, size_t n) {
    free_mem();

    this->n = n;

    // Инициализация массива сплайнов
    splines = new spline_tuple[n];
    for (size_t i = 0; i < n; ++i)
    {
        splines[i].x = x[i];
        splines[i].a = y[i];
    }
    splines[0].c = 0.;

    // Решение СЛАУ относительно коэффициентов сплайнов c[i] методом прогонки для трехдиагональных матриц
    // Вычисление прогоночных коэффициентов - прямой ход метода прогонки
    float32_t *alpha = new float32_t[n - 1];
    float32_t *beta = new float32_t[n - 1];
    float32_t A, B, C, F, h_i, h_i1, z;
    alpha[0] = beta[0] = 0.;
    for (size_t i = 1; i < n - 1; ++i)
    {
        h_i = x[i] - x[i - 1], h_i1 = x[i + 1] - x[i];
        A = h_i;
        C = 2. * (h_i + h_i1);
        B = h_i1;
        F = 6. * ((y[i + 1] - y[i]) / h_i1 - (y[i] - y[i - 1]) / h_i);
        z = (A * alpha[i - 1] + C);
        alpha[i] = -B / z;
        beta[i] = (F - A * beta[i - 1]) / z;
    }

    splines[n - 1].c = (F - A * beta[n - 2]) / (C + A * alpha[n - 2]);

    // Нахождение решения - обратный ход метода прогонки
    for (size_t i = n - 2; i > 0; --i)
        splines[i].c = alpha[i] * splines[i + 1].c + beta[i];

    // Освобождение памяти, занимаемой прогоночными коэффициентами
    delete[] beta;
    delete[] alpha;

    // По известным коэффициентам c[i] находим значения b[i] и d[i]
    for (size_t i = n - 1; i > 0; --i)
    {
        float32_t h_i = x[i] - x[i - 1];
        splines[i].d = (splines[i].c - splines[i - 1].c) / h_i;
        splines[i].b = h_i * (2. * splines[i].c + splines[i - 1].c) / 6. + (y[i] - y[i - 1]) / h_i;
    }
}

float32_t cubic_spline::f(float32_t x) const {
    if (!splines) return NAN; // Если сплайны ещё не построены - возвращаем NaN

    spline_tuple *s;
    if (x <= splines[0].x) {
        // Если x меньше точки сетки x[0] - пользуемся первым эл-том массива
        s = splines + 1;
    }
    else if (x >= splines[n - 1].x) {
        // Если x больше точки сетки x[n - 1] - пользуемся последним эл-том массива
        s = splines + n - 1;
    } else {
        // Иначе x лежит между граничными точками сетки - производим бинарный поиск нужного эл-та массива
        size_t i = 0, j = n - 1;
        while (i + 1 < j) {
            size_t k = i + (j - i) / 2;
            if (x <= splines[k].x) j = k;
            else i = k;
        }
        s = splines + j;
    }

    float32_t dx = (x - s->x);
    return s->a + (s->b + (s->c / 2. + s->d * dx / 6.) * dx) * dx; // Вычисляем значение сплайна в заданной точке.
}

float32_t fabs(float32_t x) {
    if (x < 0) return -x;
    else return x;
}

int8_t fsgn(float32_t x) {
    if (x < 0) return -1;
    else if (x > 0) return 1;
    else return 0;
}

float32_t cubic_spline::invf(float32_t y, float32_t a, float32_t b) const {
    if (fsgn(f(a)) != fsgn(f(b))) {
        float32_t c = (a + b) / 2;
        if (fabs(f(c) - y) < 0.1) return c;
        else if (fsgn(f(a)) != fsgn(f(c))) return invf(y, a, c);
        else if (fsgn(f(c)) != fsgn(f(b))) return invf(y, c, b);
        else return NAN;
    }
    else return NAN;
}

void cubic_spline::free_mem() {
    delete[] splines;
    splines = nullptr;
}