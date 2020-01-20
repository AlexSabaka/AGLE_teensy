#include "Sound.h"

// temperature [Celsius] - temperature
// humidity [0..100%] - relative humidity
// pressure [kPa] - pressure
double calculateSpeedOfSound(double temperature, double humidity, double pressure)
{
  /* Check that sensible numbers were entered */
  if ((humidity > 100.0) || (humidity < 0.0)) {
    return NAN;
  } else {
    double P = pressure * 1000.0;
    /* Measured ambient temp */
    /* Molecular concentration of water vapour calculated from Rh */
    /* using Giacomos method by Davis (1991) as implemented in DTU report 11b-1997 */
    /* These commented lines correspond to values used in Cramer (Appendix) */
    /* PSV1 = sqr(T_kel)*1.2811805*pow(10,-5)-1.9509874*pow(10,-2)*T_kel ; */
    /* PSV2 = 34.04926034-6.3536311*pow(10,3)/T_kel; */
    double Xw = humidity * ((3.141593E-8 * P + 1.00062) + temperature * temperature *
                     5.6 * 1.0E-7) * (pow(2.7182818284590451, (273.15 +
      temperature) * (273.15 + temperature) * 1.2378847 * 9.9999999999999991E-6
      - 0.019121316 * (273.15 + temperature)) * pow(2.7182818284590451,
      33.93711047 - 6343.1645000000008 / (273.15 + temperature))) / P / 100.0;

    /* Xc = 314.0*pow(10,-6); */
    /* Speed calculated using the method */
    /* of Cramer from JASA vol 93 p. 2510 */
    double C = ((((0.603055 * temperature + 331.5024) - temperature * temperature *
           5.28 * 0.0001) + ((0.1495874 * temperature + 51.471935) - temperature
           * temperature * 7.82 * 0.0001) * Xw) + (((-1.82E-7 + 3.73E-8 * temperature)
           - temperature * temperature * 2.93 * 1.0E-10) * P +
          ((-85.20931 - 0.228525 * temperature) + temperature * temperature *
           5.91 * 9.9999999999999991E-6) * 0.00039999999999999996)) - (((Xw * Xw
      * 2.835149 - P * P * 2.15 * 1.0E-13) + 4.6687619199999992E-6) +
      0.00048600000000000005 * Xw * P * 0.00039999999999999996);
    return C;
  }
}