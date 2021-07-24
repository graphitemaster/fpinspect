# Floating point expression inspector

The following tool lets you inspect the computational flow of a floating point
expression, seeing where rounding occurs, when exceptions are triggered,
when precision may be lost, when special values propagate, when error
accumulates, and other floating point headaches.

# Example
```
[fpinspect]# ./fpinspect "sqrt(45.0*e+phi)/pi"
Exception: 0 (1 roundings) INEXACT (45.000000 * e)
  Trace (1 operations) MUL

Exception: 0 (1 roundings) INEXACT phi
  Trace (1 operations) MUL

Exception: 0 (2 roundings) INEXACT ((45.000000 * e) + phi)
Exception: 1 (2 roundings) INEXACT ((45.000000 * e) + phi)
  Trace (2 operations) MUL ADD

Exception: 0 (3 roundings) INEXACT sqrt(((45.000000 * e) + phi))
Exception: 1 (3 roundings) INEXACT sqrt(((45.000000 * e) + phi))
Exception: 2 (3 roundings) INEXACT sqrt(((45.000000 * e) + phi))
  Trace (3 operations) MUL ADD ADD

Exception: 0 (3 roundings) INEXACT pi
Exception: 1 (3 roundings) INEXACT pi
Exception: 2 (3 roundings) INEXACT pi
  Trace (3 operations) MUL ADD ADD

Exception: 0 (4 roundings) INEXACT (sqrt(((45.000000 * e) + phi)) / pi)
Exception: 1 (4 roundings) INEXACT (sqrt(((45.000000 * e) + phi)) / pi)
Exception: 2 (4 roundings) INEXACT (sqrt(((45.000000 * e) + phi)) / pi)
Exception: 3 (4 roundings) INEXACT (sqrt(((45.000000 * e) + phi)) / pi)
  Trace (4 operations) MUL ADD ADD DIV

(sqrt(((45.000000 * e) + phi)) / pi)
        ans: 3.54370117187500
        err: 0.00000126456894
```

As you can see, the expression `sqrt(45.0*e+phi)/pi` produces a lot of output,
each empty-line-separated region is a subexpression which triggered an exception,
in this case because `45 * e` is an inexact value, the inexact exception is
presented first. Here you can see that such an expression involved `1 operations`,
total and in this case the operation is just a `MUL`. We can also see that the
resulting expression, because it's inexact, incurred one rounding.

Following down the exception list, we can see that the exception propagated
to `phi` in a `MUL` (which is also an inexact value), and continued, with each
new inexact subexpression resulting in several roundings. Since kernels like
`sqrt` might themselves use operations like `add`, we also see the final group
of exceptions contains an additional `ADD` in it's trace.

The final result of the expression is given in `ans:` and below that you will
find the accumulative error `err:` of evaluating that expression, in this case
this function is exact to five mantissa digits of precision, out of a total of
seven, which means this expression has ~0.71 ULP of error.

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
`float32.{h,c}` and 64-bit double-precision floating-point `float64.{h,c}`, as
double-precision is necessary for 32-bit single-precision kernels
`kernel32.{h,c}` to produce correctly rounded and truncated results
to <= 1 ULP of error.

64-bit double-precision floating-point makes use of 128-bit modular arithmetic
implemented in `uint128.{h,c}`

Accumulative error accounting is handled by `real32.{h,c}` and `real64.{h,c}`
for single-precision and double-precision floating-point, respectively.

> NOTE:
>
> There are currently no 64-bit kernels, as that would require either 80-bit 
extended-precision floating-point, or 128-bit quadruple-precision floating-point
to be implemented in software to have the precision necessary to produce
correctly rounded and truncated results to <= 1 ULP of error.