! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM maxminloctest

  INTRINSIC maxloc, minloc
  integer i_arr(5)
  real r_arr(5)

  i_arr = (/ 4, 7, 2, 1, 0 /)
  i = maxloc(i_arr, 1) ! CHECK: icmp sgt i32
  continue             ! CHECK: br i1

  i = minloc((/ 0, 5, 42, -54, 1 /), 1)

  r_arr = 1.0
  i = maxloc(r_arr, 1)

  i = maxloc(r_arr + (/ 1.0, 0.0, 2.0, 3.0, 4.0 /), 1)

  i = maxloc(r_arr(:3), 1)

  ! FIXME: add codegen for other variations of max/min loc

END
