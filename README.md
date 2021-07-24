# Floating point expression inspector

The following tool lets you inspect the computational flow of a floating point
expression, seeing where rounding occurs, when exceptions are triggered,
when precision may be lost, when special values propagate, when error
accumulates, and other floating point headaches.

# Example
```
[fpinspect]# ./fpinspect "(10*e+10)/0"
Exception: 0 (1 roundings) INEXACT ((10.000000 * e) + 10.000000)

Exception: 0 (1 roundings) INEXACT 0.000000

Exception: 0 (1 roundings) INEXACT (((10.000000 * e) + 10.000000) / 0.000000)
Exception: 1 (1 roundings) DIVBYZERO (((10.000000 * e) + 10.000000) / 0.000000)

(((10.000000 * e) + 10.000000) / 0.000000)
        = inf
```
As can be seen here, the expression: `10*e+10` produces an inexact exception.
This inexactness propagates to the zero used for the divide as can be seen
in the next exception. The division by zero produces another exception, 
giving the full expression two exceptions. The result is `inf` as required by
IEEE. You can also see for each subexpression, how many roundings occured,
inexact values always require rounding.

# Documentation
Run the program with no expression or `-h` to see the options.

Here's some constants and functions available for use in expressions.
### Constants
  * e
  * pi
  * phi

### Functions
  * floor
  * ceil
  * trunc
  * sqrt
  * abs
  * min
  * max
  * copysign

# How it works
This program implements IEEE-754 floating point completely in software, emulating
all rounding modes, exceptions, and tininess detection methods which can be
configured when evaluating an expression. With exception to transcendental
functions, all floating point computation is also accurate to <= 1 ULP of error.

Currently there is support for 32-bit single-precision floating-point
`soft32.{h,c}` and 64-bit double-precision floating-point `soft64.{h,c}`, as
double-precision is necessary for 32-bit single-precision kernels
`kernel32.{h,c}` to produce correctly rounded and truncated results
to <= 1 ULP of error.

64-bit double-precision floating-point makes use of 128-bit modular arithmetic
implemented in `uint128.{h,c}`

> NOTE:
>
> There are currently no 64-bit kernels, as that would require either 80-bit 
extended-precision floating-point, or 128-bit quadruple-precision floating-point
to be implemented in software to have the precision necessary to produce
correctly rounded and truncated results to <= 1 ULP of error.