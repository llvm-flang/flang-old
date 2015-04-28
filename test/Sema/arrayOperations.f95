! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-dump %s 2>&1 | %file_check %s

SUBROUTINE foo(I_ARR)
  INTEGER I_ARR(*)
END

SUBROUTINE faa(I_ARR, N)
  INTEGER N, I_ARR(*), I(10)
  I = I_ARR + 2 ! expected-error {{use of an array expression with an implied dimension specification}}
  I = 2 + I_ARR ! expected-error {{use of an array expression with an implied dimension specification}}
  I = I_ARR(:N) * 2
END

SUBROUTINE bar(I_MAT)
  INTEGER I_MAT(4,*)
END

PROGRAM arrtest
  INTEGER I
  INTEGER I_ARR(5), I_MAT(4,4), I_CUBE(4,8,16)
  REAL R_ARR(5)
  LOGICAL L_ARR(5)
  CHARACTER*10 C_ARR(5)

  I_ARR = 0
  I_MAT = 0
  I_ARR = -I_ARR    ! CHECK: (-i_arr)
  R_ARR = I_ARR * 2 ! CHECK: real((i_arr*2))
  R_ARR = 2 * I_ARR ! CHECK: real((2*i_arr)
  R_ARR = R_ARR + 2 ! CHECK: (r_arr+real(2))
  R_ARR = R_ARR ** 2.0 / R_ARR(:) ! CHECK: ((r_arr**2)/r_arr(:))
  R_ARR = 2 - R_ARR(1:5) ! CHECK: (real(2)-r_arr(1:5))
  R_ARR = R_ARR * R_ARR ! CHECK: (r_arr*r_arr)
  R_ARR = R_ARR - I_ARR ! CHECK: (r_arr-real(i_arr))
  I_ARR = I_MAT(:,1) ** R_ARR ! CHECK: int((real(i_mat(:, 1))**r_arr))

  L_ARR = .false.
  L_ARR = L_ARR .AND. .true.
  L_ARR = L_ARR .OR. L_ARR
  L_ARR = L_ARR .EQV. L_ARR .NEQV. .false.

  C_ARR = 'Hello'
  C_ARR = C_ARR // ' World'

  L_ARR = I_ARR == 0
  L_ARR = I_ARR /= R_ARR
  L_ARR = L_ARR .AND. I_ARR <= R_ARR

  CALL foo(I_ARR+2) ! CHECK: foo(ImplicitTempArrayExpr((i_arr+2)))
  CALL foo(2*I_ARR) ! CHECK: foo(ImplicitTempArrayExpr((2*i_arr))
  CALL foo(-I_ARR) ! CHECK: foo(ImplicitTempArrayExpr((-i_arr)))
  CALL foo(+I_ARR) ! CHECK: foo((+i_arr))
  CALL foo(I_MAT(1,:)+I_ARR) ! CHECK: foo(ImplicitTempArrayExpr((i_mat(1, :)+i_arr)))

  I_ARR = .not. I_ARR ! expected-error {{invalid operand to a logical unary expression ('integer')}}
  L_ARR = - L_ARR ! expected-error {{invalid operand to an arithmetic unary expression ('logical')}}
  I_ARR = I_ARR + .false. ! expected-error {{invalid operands to an arithmetic binary expression ('integer' and 'logical')}}

  I_MAT = 0
  I_MAT = I_MAT + I_ARR ! expected-error {{conflicting shapes in an array expression (2 dimensions and 1 dimension)}}
  I_ARR = R_ARR + I_CUBE ! expected-error {{conflicting shapes in an array expression (1 dimension and 3 dimensions)}}

ENDPROGRAM arrtest
