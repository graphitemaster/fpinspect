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

# Example
```
[fpinspect]# ./fpinspect "(10*e+10)/0"
Exception: INEXACT ((10.000000 * e) + 10.000000)
Exception: DIVBYZERO|INEXACT (((10.000000 * e) + 10.000000) / 0.000000)
(((10.000000 * e) + 10.000000) / 0.000000)
 = inf
```
As can be seen here, the expression: `10*e+10` produces an inexact number,
the division by zero triggers another exception on the whole expression. The
`inf` result as required by IEEE is provided.

# Documentation
```
./fpinspect [OPTION]... [EXPRESSION]
-r   rounding mode
      0 - nearest even [default]
      1 - to zero
      2 - down
      3 - up
-t   tininess detection mode
      0 - before rounding [default]
      1 - after rounding
```