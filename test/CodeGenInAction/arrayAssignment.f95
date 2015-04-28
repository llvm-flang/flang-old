! RUN: %flang -interpret %s | %file_check %s

integer function foo(i)
  integer i
  foo = i
  i = i + 1
end

program test

  integer i_mat(3,3), i_mat2(3,3), i_mat3(-1:1,-1:1)
  real r_mat(3,3)
  integer i
  data i_mat2 / 1, 0, 0, 0, 1, 0, 0, 0, 1 /

  i = 42
  i_mat = i

  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK: 42, 42, 42, 42, 42, 42, 42, 42, 42

  i_mat = foo(i)
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3), ', ', i
  continue ! CHECK-NEXT: 42, 42, 42, 42, 42, 42, 42, 42, 42, 43

  i_mat = i_mat2
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 1, 0, 0, 0, 1, 0, 0, 0, 1

  i_mat = 1.0
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 1, 1, 1, 1, 1, 1, 1, 1, 1

  r_mat = 2.0
  r_mat(1,1) = 3.0
  r_mat(2,2) = 4.0
  i_mat = r_mat
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 3, 2, 2, 2, 4, 2, 2, 2, 2

  i_mat = 0
  i_mat(2:,:) = 1
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 0, 1, 1, 0, 1, 1, 0, 1, 1

  i_mat(:,2:) = 2
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 0, 1, 1, 2, 2, 2, 2, 2, 2

  i_mat(:1,:1) = 11
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 11, 1, 1, 2, 2, 2, 2, 2, 2

  i_mat(3:3,1:2) = 42
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 11, 1, 42, 2, 2, 42, 2, 2, 2

  i_mat(3:3,3:3) = 42
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 11, 1, 42, 2, 2, 42, 2, 2, 42

  i_mat(1,:) = 0
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 0, 1, 42, 0, 2, 42, 0, 2, 42

  i_mat(:2,2) = 21
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 0, 1, 42, 21, 21, 42, 0, 2, 42

  i_mat(:3:2,1:3:1) = 13
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 13, 1, 13, 13, 21, 13, 13, 2, 13

  i_mat(:3:3,1:3:1) = 7
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 7, 1, 13, 7, 21, 13, 7, 2, 13

  i_mat(2,:3:2) = -2
  print *, i_mat(1,1), ', ', i_mat(2,1), ', ', i_mat(3,1), ', ', &
           i_mat(1,2), ', ', i_mat(2,2), ', ', i_mat(3,2), ', ', &
           i_mat(1,3), ', ', i_mat(2,3), ', ', i_mat(3,3)
  continue ! CHECK-NEXT: 7, -2, 13, 7, 21, 13, 7, -2, 13

  i_mat3 = 0
  print *, i_mat3(-1,-1), ', ', i_mat3(0,-1), ', ', i_mat3(1,-1), ', ', &
           i_mat3(-1,0), ', ', i_mat3(0,0), ', ', i_mat3(1,0), ', ', &
           i_mat3(-1,1), ', ', i_mat3(0,1), ', ', i_mat3(1,1)
  continue ! CHECK-NEXT: 0, 0, 0, 0, 0, 0, 0, 0, 0

  i_mat3(-1:0,0:1) = 12
  print *, i_mat3(-1,-1), ', ', i_mat3(0,-1), ', ', i_mat3(1,-1), ', ', &
           i_mat3(-1,0), ', ', i_mat3(0,0), ', ', i_mat3(1,0), ', ', &
           i_mat3(-1,1), ', ', i_mat3(0,1), ', ', i_mat3(1,1)
  continue ! CHECK-NEXT: 0, 0, 0, 12, 12, 0, 12, 12, 0

  i_mat3(:1:2,:) = 66
  print *, i_mat3(-1,-1), ', ', i_mat3(0,-1), ', ', i_mat3(1,-1), ', ', &
           i_mat3(-1,0), ', ', i_mat3(0,0), ', ', i_mat3(1,0), ', ', &
           i_mat3(-1,1), ', ', i_mat3(0,1), ', ', i_mat3(1,1)
  continue ! CHECK-NEXT: 66, 0, 66, 66, 12, 66, 66, 12, 66

  i_mat3(0,:1:1) = 5
  print *, i_mat3(-1,-1), ', ', i_mat3(0,-1), ', ', i_mat3(1,-1), ', ', &
           i_mat3(-1,0), ', ', i_mat3(0,0), ', ', i_mat3(1,0), ', ', &
           i_mat3(-1,1), ', ', i_mat3(0,1), ', ', i_mat3(1,1)
  continue ! CHECK-NEXT: 66, 5, 66, 66, 5, 66, 66, 5, 66

end
