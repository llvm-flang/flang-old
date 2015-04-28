! RUN: %flang -fsyntax-only -verify < %s

PROGRAM arrayIntrinsics
  intrinsic maxloc, minloc

  integer i_mat(10,10), i_arr(10)
  logical l_mat(10,10), l_mat2(2,2), l_arr(100)
  real r_mat(10,10)
  complex c_mat(10,10)


  integer i_pair(2), i_triple(3), i

  i_mat = 0
  r_mat = 0
  c_mat = 0
  l_mat = .true.

  ! MAXLOC/MINLOC
  ! FIXME: add support for optional dimension parameter
  ! FIXME: add test for invalid second parameter (diag referencing both dim and mask)

  i_pair = maxloc(i_mat)
  i_pair = minloc(r_mat)

  i = maxloc(i_arr, 1)
  i_pair = minloc(i_arr,1)

  i_pair = maxloc(i_mat, l_mat)

  i = minloc(i_arr, 2.0) ! expected-error {{passing 'real' to parameter 'dim' of incompatible type 'integer' (or parameter 'mask' of type 'logical array')}}
  i_pair = maxloc(i_mat, l_arr) ! expected-error {{conflicting shapes in arguments 'array' and 'mask' (2 dimensions and 1 dimension)}}
  i_pair = maxloc(i_mat, l_mat2) ! expected-error {{conflicting size for dimension 1 in arguments 'array' and 'mask' (10 and 2)}}
  i_pair = maxloc(c_mat) ! expected-error {{passing 'complex array' to parameter 'array' of incompatible type 'integer array' or 'real array'}}
  i_pair = minloc(l_mat) ! expected-error {{passing 'logical array' to parameter 'array' of incompatible type 'integer array' or 'real array'}}
  i_pair = maxloc(i)     ! expected-error {{passing 'integer' to parameter 'array' of incompatible type 'integer array' or 'real array'}}

  i = minloc(i_mat)      ! expected-error {{assigning to 'integer' from incompatible type 'integer array'}}
  i_triple = maxloc(i_mat) ! expected-error {{conflicting size for dimension 1 in an array expression (3 and 2)}}
  i_triple = maxloc(i_arr) ! expected-error {{conflicting size for dimension 1 in an array expression (3 and 1)}}

END PROGRAM
