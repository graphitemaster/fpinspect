# Floating point expression inspector

The following tool lets you inspect the computational flow of a floating point
expression, seeing where rounding occurs, when exceptions are triggered, and
when precision may be lost.

# How it works
This program implements IEEE-754 floating point completely in software, emulating
all rounding modes, exceptions, and tininess detection methods which can be
configured when evaluating an expression. With exception to transcendental
functions, all floating point computation is also accurate to <= 1 ULP of error.

# Constants
The following constants exist to 128-bits of precision
  * e
  * pi
  * phi

# Functions
The following functions exist to 128-bits of precision
  * floor
  * ceil
  * trunc
  * sqrt
  * abs
  * min
  * max
  * copysign

