#include "Filter.h"

int16_t lp_coeffs_1000[100] = {
    #include "low_pass_cut_1000Hz.h"
};
int16_t lp_coeffs_800[42] = {
    #include "low_pass_cut_800Hz.h"
};
int16_t bp_coeffs_1000_2000[200] = {
    #include "band_pass_1kHz_2kHz.h"
};

fir_filter low_pass_cut_1000Hz = {
    lp_coeffs_1000,
    100 
};

fir_filter low_pass_cut_800Hz = {
    lp_coeffs_800,
    42 
};

fir_filter band_pass_1kHz_2kHz = {
    bp_coeffs_1000_2000,
    200
};