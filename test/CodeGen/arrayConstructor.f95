! RUN: %flang -emit-llvm -o - %s | %file_check %s

PROGRAM test ! CHECK: private constant [4 x i32] [i32 1, i32 2, i32 3, i32 4]

  integer i_arr(4), i_mat(4,4)
  logical l_arr(4)
  integer i
  parameter(i = 0)
  integer n


  i_arr = (/ 1, 2, 3, 4 /)
  i_arr = (/ i, i, 2, 4 /)
  n = 42
  i_arr = (/ n, i, 2, n /)

  i_arr = i_arr + (/ 0, 1, 2, 3 /)

  l_arr = (/ .false., .true., .false., i == n /)

END
