! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

SUBROUTINE SUB(ARR, ARR2, ARR3)
  INTEGER ARR(*)
  INTEGER ARR2(*,*) ! expected-error {{the dimension declarator '*' must be used only in the last dimension}}
  REAL ARR3(10,*)
  INTEGER i

  i = ARR(1) ! CHECK: i = arr(1)
  i = ARR3(3,2) ! CHECK: i = int(arr3(3, 2))
END

SUBROUTINE BUS(L, J, A, ARR, ARR2, ARR3, ARR4, ARR5)
  INTEGER L, J
  INTEGER I
  REAL A
  REAL ARR2(L)
  DIMENSION ARR(L)
  REAL ARR3(L+J+1)
  REAL ARR4(L+A) ! expected-error {{array specifier requires an integer argument ('real' invalid)}}
  REAL ARR5(L+I) ! expected-error {{expected a constant expression}}

  ! ARR is REAL by implicit
  I = ARR(1) ! CHECK: i = int(arr(1))
  I = ARR2(1) ! CHECK: i = int(arr2(1))
END

SUBROUTINE USB(LENGTH)
  INTEGER LENGTH
  INTEGER M_ARM  ! expected-note {{declared here}}
  DIMENSION M_ARM(*) ! expected-error {{use of dimension declarator '*' for a local variable 'm_arm'}}

  REAL X_ARM(LENGTH)
  INTEGER I
  I = LENGTH
  X_ARM(1) = 1.0
END

PROGRAM arrtest
  INTEGER I_ARR(30, 10:20, 20)
  INTEGER I_ARR2(I_ARR(1,2,3)) ! expected-error {{expected an integer constant expression}}
  INTEGER I_ARR3(.false.:2) ! expected-error {{expected an integer constant expression}}

  INTEGER I_ARRM(*) ! expected-error {{use of dimension declarator '*' for a local variable 'i_arrm'}}
  INTEGER I ! expected-note@-1 {{declared here}}

ENDPROGRAM arrtest
