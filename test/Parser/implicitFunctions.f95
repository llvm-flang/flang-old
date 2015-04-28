! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

PROGRAM imptest
  integer i
  real func
  real func2
  real func3(10)

  i = 0
  func2 = 1
  i = abs(i) ! CHECK: i = abs(i)
  i = foo(i) ! CHECK: i = int(foo(i))
  i = func(i)! CHECK: i = int(func(i))
  i = func3(i) ! CHECK: i = int(func3(i))
  i = func2(i) ! expected-error {{unexpected '('}}
END
