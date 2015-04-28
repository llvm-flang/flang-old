! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-print %s 2>&1 | %file_check %s

PROGRAM datatest
  INTEGER I, J, K, M, NNN, O, P, Q
  REAL X,Y,Z, A, ZZZ
  INTEGER I_ARR(10)
  INTEGER I_ARR2(2,2)
  REAL R_ARR(10), R_ARR2(3), R_MULTIARR(2,3,4)
  CHARACTER*(10) STR, STR_ARR(11)

  type point
    integer x,y
  end type
  type(point) point1, point2, pointArr(4)

  PARAMETER (PI = 3.14, INDEX = 1)

  DATA I / 1 /
  DATA J, K / 2*42 / M / -11 /

  DATA X, Y, Z / 0*11 / ! expected-error {{expected an integer greater than 0}}
  DATA X, Y / 2*ZZZ / ! expected-error {{expected a constant expression}}
  DATA X, Y / 2, J / ! expected-error {{expected a constant expression}}

  DATA X / PI /

  DATA X / 1 /

  DATA NNN / 1, 234, 22 / ! expected-error {{excess values in a 'data' statement}}
  DATA NNN / 2*2 /  ! expected-error {{excess values in a 'data' statement}}

  DATA O, P / 0 / ! expected-error {{not enough values in a 'data' statement}}

  DATA A / .false. / ! expected-error {{initializing 'real' with an expression of incompatible type 'logical'}}
  DATA NNN / 'STR' / ! expected-error {{initializing 'integer' with an expression of incompatible type 'character'}}

  DATA R_ARR(1) / 1.0 / R_ARR(2), R_ARR(3) / 2*0.0 /

  DATA R_ARR(4) / .false. / ! expected-error {{initializing 'real' with an expression of incompatible type 'logical'}}

  DATA STR / 'Hello' / STR_ARR(1)(:), STR_ARR(2) / 2*'World' /
  DATA STR_ARR(3)(2:5) / 'STR' /

  DATA STR_ARR(4)(:4) / 1 / ! expected-error {{initializing 'character' with an expression of incompatible type 'integer'}}

  DATA R_ARR / 10*1.0 /

  DATA R_ARR / 5*1.0 / ! expected-error {{not enough values in a 'data' statement}}

  DATA R_ARR / 11*1.0 / ! expected-error {{excess values in a 'data' statement}}

  DATA R_ARR2 / 1, .false., 2.0 / ! expected-error {{initializing 'real' with an expression of incompatible type 'logical'}}

  DATA R_ARR2 / 1.5, 2*-1.0 /

  DATA R_MULTIARR / 24*88.0 /

  DATA (I_ARR(I), I = 1,10) / 10*0 /
  DATA (I_ARR(I), I = 0 + 1, 20/2) / 10*2 /

  DATA (I_ARR(WHAT), I = 1,10) / 10*0 / ! expected-error {{use of undeclared identifier 'what'}}

  DATA (ZZZ, I = 1,10) / 1 / ! expected-error {{expected an implied do or an array element expression}}

  DATA (I_ARR(I), I = 1, .true.) / 10*0 / ! expected-error {{expected an integer constant or an implied do variable expression}}

  DATA ((I_ARR2(I,J), J = 1,2), I = 1,2) / 4*3 /

  DATA ((I_ARR2(I,J), J = 1,2), I_ARR(I), I = 1,2) / 4*3, 2*0 /
  DATA ((I_ARR2(I,J), J = 1,2), I_ARR(I), I = 1,2) / 5*0 / ! expected-error {{not enough values in a 'data' statement}}

  DATA ((I_ARR2(I,J), J = 1,2), I_ARR(J), I = 1,2) / 4*3, 2*0 / ! expected-error {{expected an integer constant or an implied do variable expression}}
  DATA (I_ARR(I+1), I=1,10) / 10*9 / ! expected-error {{expected an integer constant expression}}

  DATA (I_ARR(I), I = INDEX, 10) / 10 * 0 /
  DATA (I_ARR2(INDEX,J), J = 1,2) / 2*3 /

  data point1 / Point(1,2) /
  data point2 / Point(3,4) /

  data point1%x, point1%y / 2*0 /

  data pointArr / 4*Point(-1,1) /
  data pointArr(1)%x / 13 / pointArr(2)%y / 42 /

END PROGRAM

subroutine sub                   ! CHECK: i = 1
  integer i, j, k                ! CHECK: j = 0
  data i / 1 / j, k / 2*0 /      ! CHECK: k = 0
end

subroutine sub2
  integer i_mat(2,2)
  integer i_arr(2)

  data i_mat / 2*1, 2*2 /             ! CHECK: i_mat = (/1, 1, 2, 2 /)
  data i_arr(1), i_arr(2) / 13, 42 /  ! CHECK: i_arr = (/13, 42 /)
end

subroutine sub3
  integer i_mat(2,2)
  integer i_arr(4), i_arr2(-2:2)
  integer two
  parameter (two = 2)

  data ((i_mat(i,j), j = 1,two), i = 1,2) / 4*3 /    ! CHECK: i_mat = (/3, 3, 3, 3 /)
  data (i_arr(i), i = 1,3), i_arr(4) / 11,12,13,14 / ! CHECK: i_arr = (/11, 12, 13, 14 /)
  data i_arr2(-2), (i_arr2(i), i = -1,2) / 100, 101, &
         102, 103, 104 /                             ! CHECK: i_arr2 = (/100, 101, 102, 103, 104 /)
end

subroutine sub4
  character(len=10) str1, str2, str3 ! CHECK: str1 = 'Hello     '
  character(len=5) strArr(3), strArr2(3)

  data str1 / 'Hello' / str2(:) / 'World' / ! CHECK: str2 = 'World     '
  data str3(2:4) / 'Flang' / ! CHECK: str3 = ' Fla      '

  data strArr / 3*'foobar' / ! CHECK: strarr = (/'fooba', 'fooba', 'fooba' /)
  data strArr2(1) / 'Hello' /
  data strArr2(2)(2:4) / '+' /
  data (strArr2(i), i = 3,3) / 'funke' / ! CHECK: strarr2 = (/'Hello', ' +   ', 'funke' /)
end

subroutine sub5
  type point
    integer x,y
  end type
  type(point) p1, p2, pArr(3) ! CHECK: p1 = point((-1), 1)
  type(point) pArr2(3)

  data p1 / Point(-1,1) / p2%x, p2%y / 13, 42 / ! CHECK: p2 = point(13, 42)
  data pArr / 2*Point(0,7), Point(1,1) /  ! CHECK: parr = (/point(0, 7), point(0, 7), point(1, 1) /)
  data pArr2(1)%x, pArr2(1)%y / 2*16 / ! CHECK: parr2 = (/point(16, 16), point(1, 2), point(3, 4) /)
  data pArr2(2), parr2(3) / point(1,2), point(3,4) /
  ! FIXME: TODO: data (pArr2(i)%x, pArr2(i)%y, i = 2,3) /  1, 2, 3, 4 /

end

integer function func(i)
  data i / 0 /     ! expected-error {{function argument can't be initialized by a 'data' statement}}
  data func / 12 / ! expected-error {{function result variable can't be initialized by a 'data' statement}}
end
