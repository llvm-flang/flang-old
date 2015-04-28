! RUN: %flang -interpret %s | %file_check %s

program eqtest
  integer i, j
  real x
  equivalence(i, j, x)

  integer is, i_mat(3,3), i_mat2(3,3)
  equivalence (is, i_mat), (i_mat, i_mat2(2,1))

  real c_elements(2)
  complex c
  equivalence (c, c_elements(1))

  print *, 'START' ! CHECK: START
  i = 42
  print *, i ! CHECK-NEXT: 42
  print *, j ! CHECK-NEXT: 42
  i = 0
  print *, i ! CHECK-NEXT: 0
  print *, j ! CHECK-NEXT: 0

  x = 2.5
  if(x >= 2.5) print *, 'yes' ! CHECK-NEXT: yes
  if(i /= 0) print *, 'yeah'  ! CHECK-NEXT: yeah

  is = 21
  print *, i_mat(1,1)  ! CHECK-NEXT: 21
  print *, i_mat2(2,1) ! CHECK-NEXT: 21
  i_mat2(2,1) = -66
  print *, is          ! CHECK-NEXT: -66
  print *, i_mat(1,1)  ! CHECK-NEXT: -66

  c = (1.0 , 2.0)
  print *, int(c_elements(1)) ! CHECK-NEXT: 1
  print *, int(c_elements(2)) ! CHECK-NEXT: 2

end
