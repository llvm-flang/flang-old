! RUN: %flang -fsyntax-only -verify < %s

PROGRAM test
  intrinsic etime

  real tarr(2), ttriple(3)
  integer iarr(2)
  real result

  result = etime(tarr)
  result = etime(iarr)    ! expected-error {{passing 'integer array' to parameter 'tarray' of incompatible type 'real array'}}
  result = etime(ttriple) ! expected-error {{passing array with dimension 1 of size 3 to argument 'tarray' of incompatible size 2}}

END PROGRAM

SUBROUTINE FOO
  real etime
  intrinsic etime

  real tarr(2)
  result = etime(tarr)
END
