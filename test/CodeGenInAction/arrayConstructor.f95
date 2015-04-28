! RUN: %flang -interpret %s | %file_check %s

program test

  integer i_arr(4), i_mat(4,4), i_arr2(2)
  logical l_arr(4)
  integer i
  parameter(i = 0)
  integer n

  print *, 'START' ! CHECK: START

  i_arr = (/ 1, 2, i, 4 /)
  print *, i_arr(1), ', ', i_arr(2), ', ', i_arr(3), ', ', i_arr(4)
  continue ! CHECK-NEXT: 1, 2, 0, 4

  n = 42
  i_arr = (/ n, i, 2, n /)
  print *, i_arr(1), ', ', i_arr(2), ', ', i_arr(3), ', ', i_arr(4)
  continue ! CHECK-NEXT: 42, 0, 2, 42

  i_arr = i_arr * (/ 0, 1, n, 3 /) + (/ i, 1, 2, 3 /)
  print *, i_arr(1), ', ', i_arr(2), ', ', i_arr(3), ', ', i_arr(4)
  continue ! CHECK-NEXT: 0, 1, 86, 129

  l_arr = (/ .true., n == 42, .false., n < 1 /)
  print *, l_arr(1), ', ', l_arr(2), ', ', l_arr(3), ', ', l_arr(4)
  continue ! CHECK-NEXT: true, true, false, false

  i_arr2 = 11
  i_arr = (/ i_arr2, i_arr2 /)
  print *, i_arr(1), ', ', i_arr(2), ', ', i_arr(3), ', ', i_arr(4)
  continue ! CHECK-NEXT: 11, 11, 11, 11

end
