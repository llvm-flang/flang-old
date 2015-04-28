! RUN: %flang -interpret %s | %file_check %s

subroutine sub(f, g)
  external f
  integer g
  external g

  call f(2)
  print *, g(3.0)
end

subroutine f(i)
  integer i
  print *, i
end

integer function foo(r)
  foo = int(r) + 1
end

program test        ! CHECK: START
  print *, 'START'  ! CHECK-NEXT: 2
  call sub(f, foo)  ! CHECK-NEXT: 4
end
