! RUN: %flang -fsyntax-only -verify < %s
PROGRAM arrtest
  DIMENSION I_ARR2(1,2,3,4)
  INTEGER I_ARR(30)
  REAL MATRIX(4,4)
  LOGICAL SET(10:20)
  INTEGER I_ARR2
  REAL, DIMENSION(2,2) :: MATRIX2
  INTEGER I_SCAL


  I_ARR(1) = 2
  I_ARR(2) = I_ARR(1)
  I_ARR(3) = 4
  I_ARR(4) = I_ARR(3 ! expected-error {{expected ')'}}
  I_ARR(4) = I_ARR(  ! expected-error {{expected an expression after '('}}
  I_ARR(4) = I_ARR(1, ! expected-error {{expected an expression after ','}}
  I_ARR2(1,1,1,1) = 3
  I_ARR(4) = I_ARR( / ) ! expected-error {{expected an expression}}
  I_ARR2(1,1,1,2) = I_ARR2(1,1,1,1)

  I_SCAL = I_SCAL(3) ! expected-error {{unexpected '('}}

  MATRIX(1,1) = 0.0
  MATRIX(2,2) = MATRIX(1,1)
ENDPROGRAM arrtest
