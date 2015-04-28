! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
PROGRAM constants
  REAL X
  DOUBLE PRECISION Y

  X = 1e+1
  X = 1e2
  X = 1E-3
  X = 1.0
  X = 1.25
  X = 1.5E+2
  X = -0.9e-4

  Y = 1d1
  Y = 2d+5
  Y = +3D-4
  Y = 0.4d4
  Y = -0.125D-2
  Y = 1.0d+2
  if(1.LT.2) then
  end if

  if(1.eq.2) then
  end if

  if(1.ne.2) then
  end if

  if(1. / 2. >= .5) then ! CHECK: (1/2)>=0.5
  end if

  X = 1e ! expected-error {{exponent has no digits}}
  Y = -2D- ! expected-error {{exponent has no digits}}
END PROGRAM constants
