! RUN: %flang -interpret %s | %file_check %s
program test
  intrinsic huge, tiny
  integer (kind=1) i8
  integer i

  ! CHECK: max(int_8)=127
  i = huge(i8)
  print *, 'max(int_8)=', i
  ! CHECK-NEXT: min(int_8)=-128
  i = tiny(i8)
  print *, 'min(int_8)=', i
  ! CHECK-NEXT: digits(int_8)=7
  print *, 'digits(int_8)=', digits(i8)
  ! CHECK-NEXT: range(int_8)=2
  print *, 'range(int_8)=', range(i8)

  ! CHECK-NEXT: precision(float)=6
  print *, 'precision(float)=', precision(1.0)
  ! CHECK-NEXT: range(float)=37
  print *, 'range(float)=', range(1.0)

  print *, 'max(int)=', huge(i)
  print *, 'tiny(int)=', tiny(i)
  print *, 'range(int)=', range(i)

end
