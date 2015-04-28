! RUN: %flang -fsyntax-only < %s
! RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
PROGRAM imptest
  !Unless specified otherwise,
  !all variables starting with letters I, J, K, L, M and N are default INTEGERs,
  !and all others are default REAL

  I = 22 ! CHECK: i = 22
  J = I ! CHECK: j = i
  k = J ! CHECK: k = j
  M = 0.0 ! CHECK: m = int(0)
  n = -1 ! CHECK: n = (-1)

  R = 33.25 ! CHECK: r = 33.25
  Z = 1 ! CHECK: z = real(1)
  a = -11.23
  x = y ! CHECK: x = y
END PROGRAM imptest
