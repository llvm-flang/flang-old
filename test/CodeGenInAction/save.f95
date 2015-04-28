! RUN: %flang -interpret %s | %file_check %s

function bar(i)
  integer k, bar
  data k / -1 /
  save k
  bar = k
  k = i
end

program test

  print *,'START'  ! CHECK: START
  print *, bar(1)  ! CHECK-NEXT: -1
  print *, bar(42) ! CHECK-NEXT: 1
  print *, bar(-3) ! CHECK-NEXT: 42
  print *, bar(0)  ! CHECK-NEXT: -3

end
