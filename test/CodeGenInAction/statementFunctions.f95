! RUN: %flang -interpret %s | %file_check %s

INTEGER FUNCTION ONE()
  PRINT *,'ONE'
  ONE = 1
END

PROGRAM test
  J(I) = I + 1
  CHARACTER*(7) ABC
  INTEGER FOO
  FOO(A) = J(INT(A)) * 2
  ABC() = 'VOLBEAT'
  INTEGER i_arr(2)
  istmtFunc(i) = i + i

  PRINT *, J(2) ! CHECK: 3
  PRINT *, J(-1) ! CHECK-NEXT: 0
  PRINT *, FOO(2.5) ! CHECK-NEXT: 6
  PRINT *, ABC() ! CHECK-NEXT: VOLBEAT

  i_arr(1) = 1
  i_arr(2) = 3
  PRINT *, istmtFunc(i_arr(one())) ! CHECK-NEXT: ONE
  continue ! CHECK-NEXT: 2
  PRINT *, istmtFunc(i_arr(one()+1)) ! CHECK-NEXT: ONE
  continue ! CHECK-NEXT: 6

  ! FIXME: add codegen for array arguments.

END PROGRAM test
