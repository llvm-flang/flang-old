! RUN: %flang -interpret %s | %file_check %s

program datatest
  integer i, j
  integer i_arr(10)
  integer i_mat(3,3)

  data i / 1 / j / 0 /
  data (i_arr(i), i = 1,10) / 2*0, 5*2, 3*-1 /
  data ( (i_mat(i,j), i = 1,3), j = 1, 3) &
       / 3*42, 3*11, 3*-88 /


  character(len=5) str1, str2, str3
  character(len=5) strArr(2), strArr2(3)

  data str1 / 'Hello' / str2(:) / 'World' /
  data str3(2:4) / 'Flang' /

  data strArr / 'Hello', 'World' /
  data strArr2(1) / 'Hello' /
  data strArr2(2)(2:4) / '+' /
  data (strArr2(i), i = 3,3) / 'funke' /

  type point
    integer x,y
  end type
  type(point) p1, p2

  data p1 / Point(-1,1) / p2%x, p2%y / 13, 42 /

  print *, 'START' ! CHECK: START
  print *, i       ! CHECK-NEXT: 1
  print *, j       ! CHECK-NEXT: 0

  print *, i_arr(1), ', ', i_arr(3), ', ', i_arr(8), ', ', &
           i_arr(10) ! CHECK-NEXT: 0, 2, -1, -1


  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 42, 42, 42, 11, 11, 11, -88, -88, -88

  print *, str1 ! CHECK-NEXT: Hello
  print *, str2 ! CHECK-NEXT: World
  print *, str3 ! CHECK-NEXT:  Fla
  print *, strArr(1) ! CHECK-NEXT: Hello
  print *, strArr(2) ! CHECK-NEXT: World
  print *, strArr2(1) ! CHECK-NEXT: Hello
  print *, strArr2(2) ! CHECK-NEXT:  +
  print *, strArr2(3) ! CHECK-NEXT: funke

  print *, p1%x, ', ', p1%y ! CHECK-NEXT: -1, 1
  print *, p2%x, ', ', p2%y ! CHECK-NEXT: 13, 42

end program
