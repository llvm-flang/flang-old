! RUN: %flang -interpret %s | %file_check %s

real function foo()
  print *, 'foo'
  foo = 10.0
end

program maxminloctest

  intrinsic maxloc, minloc
  integer i_arr(5)
  real r_arr(5)

  print *, 'START' ! CHECK: START
  i_arr = (/ 4, 7, 2, 1, 0 /)
  i = maxloc(i_arr, 1)
  print *, i ! CHECK-NEXT: 2

  i = minloc((/ 0, 5, 42, -54, 1 /), 1)
  print *, i ! CHECK-NEXT: 4

  r_arr = (/ 1.0, 2.0, 0.0, 9.0, 9.0 /)
  i = maxloc(r_arr,1)
  print *, i ! CHECK-NEXT: 4

  i = maxloc(r_arr + (/ foo(), foo(), 1.0, 0.0, 0.0 /), 1)
  continue   ! CHECK-NEXT: foo
  continue   ! CHECK-NEXT: foo
  print *, i ! CHECK-NEXT: 2

  i = minloc(r_arr * (/ foo(), foo(), 1.0, 0.0, 0.0 /), 1)
  continue   ! CHECK-NEXT: foo
  continue   ! CHECK-NEXT: foo
  print *, i ! CHECK-NEXT: 3

  i = maxloc(r_arr(:3),1)
  print *, i ! CHECK-NEXT: 2

  i = maxloc(r_arr(2:4),1)
  print *, i ! CHECK-NEXT: 3

  i = minloc(r_arr(3:),1)
  print *, i ! CHECK-NEXT: 1

end
