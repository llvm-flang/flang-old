! RUN: %flang -fsyntax-only -verify < %s

PROGRAM test
  intrinsic radix, digits, minexponent, maxexponent
  intrinsic precision, range
  intrinsic huge, tiny, epsilon

  integer i
  real x
  logical l

  x = radix(1.0)
  i = radix(0)
  x = digits(1.0)
  i = digits(1)
  x = minexponent(1.0)

  x = precision(x)
  x = range(x)

  x = huge(x)
  i = tiny(i)
  x = epsilon(x)

  i = maxexponent(i) ! expected-error {{passing 'integer' to parameter of incompatible type 'real'}}
  i = tiny(l) ! expected-error {{passing 'logical' to parameter of incompatible type 'integer' or 'real'}}
  x = epsilon(i) ! expected-error {{passing 'integer' to parameter of incompatible type 'real'}}

END PROGRAM
