! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s
program intrinfuntest
  integer i_mat(4,4), i_arr(5)
  integer(8) i64
  real r_mat(4,4), r_arr(10)
  complex c_mat(4,4), c_arr(5)
  character char_mat(4,4)
  logical l_mat(4,4)

  INTRINSIC INT, IFIX, IDINT
  INTRINSIC REAL, FLOAT, sngl
  INTRINSIC DBLE, cmplx
  INTRINSIC char, ICHAR

  INTRINSIC AINT, dint, anint, DNINT, nint, IDNINT
  INTRINSIC abs, iabs, dabs, cabs
  INTRINSIC mod, sign, dim, dprod, max, min
  INTRINSIC len, len_trim, index
  INTRINSIC aimag, conjg

  intrinsic sqrt, dsqrt, csqrt, exp, dexp, cexp
  intrinsic log, alog, dlog, clog, log10, alog10, dlog10
  intrinsic sin, dsin, csin, cos, dcos, ccos, tan, dtan
  intrinsic asin, dasin, acos, dacos, atan, datan, atan2, datan2
  intrinsic sinh, dsinh, cosh, dcosh, tanh, dtanh

  intrinsic lge, lgt, lle, llt

!! conversion functions

  i_mat = int(1)
  i_mat = int(2.0)
  r_mat = real(i_mat)
  i_mat = int(r_mat)
  c_mat = cmplx(r_mat)
  i_mat = int(c_mat)
  c_mat = cmplx(i_mat, i_mat)
  i_arr = int( (/ 1.0, 2.0, 3.0, 4.0, 5.0 /) )
  r_arr = real( (/ i_arr, 6,7,8,9,10 /) )
  r_mat = int(c_mat) ! CHECK: r_mat = real(int(c_mat))
  r_mat = real(i_arr) ! expected-error {{conflicting shapes in an array expression (2 dimensions and 1 dimension)}}
  c_mat = cmplx(i_mat, i_arr) ! expected-error {{conflicting shapes in arguments 'x' and 'y' (2 dimensions and 1 dimension)}}
  c_arr = cmplx(i_arr, r_arr) ! expected-error {{conflicting size for dimension 1 in arguments 'x' and 'y' (5 and 10)}}

!! misc and maths functions

  i_mat = mod(i_mat,i_mat)
  i_mat = mod(i_mat,7)
  i_mat = mod(i_mat,i64) ! expected-error {{conflicting types in arguments 'x' and 'y' ('integer' and 'integer (Kind=8)')}}
  r_mat = atan2(r_mat,2.0)
  r_mat = atan2(r_mat,c_mat) ! expected-error {{conflicting types in arguments 'x' and 'y' ('real' and 'complex')}}
  r_arr = atan2(2.0, r_mat)
  r_mat = sign(r_mat, r_mat)
  i_mat = dim(4, i_mat)
  i_mat = dim(i_mat, int(12,8)) ! expected-error {{conflicting types in arguments 'x' and 'y' ('integer' and 'integer (Kind=8)')}}

  r_mat = aimag(c_mat)
  i_arr = aimag(c_mat) ! expected-error {{conflicting shapes in an array expression (1 dimension and 2 dimensions)}}
  c_mat = conjg(c_mat)
  r_arr = dprod(r_arr,r_arr)
  r_arr = dprod(r_arr,r_mat) ! expected-error {{conflicting shapes in arguments 'x' and 'y' (1 dimension and 2 dimensions)}}

  i_mat = abs(i_mat) + sqrt(r_mat)
  r_mat = sin(r_mat) * cos(r_mat) + tan(r_mat)
  c_mat = exp(c_mat) + sin(c_mat)
  r_mat = log(r_mat) * log10(r_arr) ! expected-error {{conflicting shapes in an array expression (2 dimensions and 1 dimension)}}

end
