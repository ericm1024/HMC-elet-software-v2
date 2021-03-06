1/30/2017 pressure sensor calibration

We have 2 pressure sesors, one labeled P1 with yellow tape (SN 062716D005) and
one labeled P2 with blue tape (SN 041116D204). Their calibration data is in 2
.csv files.

The first column of the data is the digital readout from the arduino pin. The
arduino uses a 10-bit ADC, so those values are in the range of 0-1023, with 0
corresponding to 0V and 1023 corresponding to 5V. The pressure sensors output
a signal in the range of 1V-5V, which corresponds linearly to the pressure
range of 0psi to 1000psi. Note: from testing it appears that it may not start
at 0psi, it may actually start at atmospheric pressure.

The second column of the data is our uncalibrated pressure, calculated
according to the following C function:

static float digital_to_psi(int digital)
{
  return ((float)(digital - 205))*2.442;
}

Note that this function is *wrong* in that it assumes the pressure range of
the sensor is 0-2000psi, so the 2.442 constant is off by a factor of two. Thus
you should ignore the second column of the data.

The calbiration_notes.png image lists the pressures we calbirated at for each
sensor.