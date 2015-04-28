! RUN: %flang -fsyntax-only -verify < %s
PROGRAM arrtest
  INTEGER I_ARR(30)
  INTEGER I_ARR2(30,20)
  LOGICAL L_ARR(16)

  I_ARR(1) = 1

  I_ARR(1) = I_ARR(1,1) ! expected-error {{array subscript must have 1 subscript expression}}
  I_ARR2(1) = I_ARR(1) ! expected-error {{array subscript must have 2 subscript expressions}}

  I_ARR(1) = I_ARR(I_ARR(1))
  I_ARR(I_ARR(I_ARR(1))) = 2

  I_ARR(1) = I_ARR('HELLO') ! expected-error {{expected an expression of integer type ('character' invalid)}}
  I_ARR(1) = I_ARR(.TRUE.) ! expected-error {{expected an expression of integer type ('logical' invalid)}}
  I_ARR(1) = I_ARR2(1,3.0) ! expected-error {{expected an expression of integer type ('real' invalid)}}
  I_ARR(1) = I_ARR((42.0,69.0)) ! expected-error {{expected an expression of integer type ('complex' invalid)}}

  L_ARR(1) = .false.
  I_ARR(5) = I_ARR(L_ARR(1)) ! expected-error {{expected an expression of integer type ('logical' invalid)}}
ENDPROGRAM arrtest
