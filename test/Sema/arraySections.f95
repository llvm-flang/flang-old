! RUN: %flang -fsyntax-only -verify < %s
! RUN: %flang -fsyntax-only -verify -ast-dump %s 2>&1 | %file_check %s

SUBROUTINE foo(I_ARR)
  INTEGER I_ARR(*)
END

SUBROUTINE bar(I_MAT)
  INTEGER I_MAT(4,*)
END

PROGRAM arrtest
  INTEGER I_ARR(5), I_MAT(4,4), I_CUBE(4,8,16)

  I_ARR = I_ARR(:)
  I_ARR(1:3) = 2
  I_ARR(2:) = 3
  I_ARR(:4) = 1
  I_ARR(:) = I_ARR(:)

  I_ARR(1:4:2) = 0

  I_MAT(:,:) = 0
  I_MAT(:,1) = 1
  I_MAT(2,1:1) = 2
  I_MAT(:, 1:4:2) = 4
  I_CUBE(:,1:4,1) = I_MAT

  CALL foo(I_ARR) ! CHECK: foo(i_arr)
  CALL foo(I_ARR(:))
  CALL foo(I_ARR(2:))
  CALL foo(I_ARR(1:5:2)) ! CHECK: foo(ImplicitArrayPackExpr(i_arr(1:5:2)))
  CALL bar(I_MAT(:,:))
  CALL bar(I_MAT) ! CHECK: bar(i_mat)
  CALL bar(I_MAT(:,1:)) ! CHECK: bar(i_mat(:, 1:))
  CALL bar(I_MAT(1:2,3:4)) ! CHECK: bar(ImplicitArrayPackExpr(i_mat(1:2, 3:4)))
  CALL bar(I_CUBE(:,:,1)) ! CHECK: bar(i_cube(:, :, 1))
  CALL bar(I_CUBE(:,1:4,1)) ! CHECK: bar(i_cube(:, 1:4, 1))
  ! FIXME: the packing below isn't required
  CALL bar(I_CUBE(1:4,1:4,1)) ! CHECK: bar(ImplicitArrayPackExpr(i_cube(1:4, 1:4, 1)))
  CALL bar(I_CUBE(1:3,1:4,1)) ! CHECK: bar(ImplicitArrayPackExpr(i_cube(1:3, 1:4, 1)))
  CALL bar(I_CUBE(1,1:4,1:4)) ! CHECK: bar(ImplicitArrayPackExpr(i_cube(1, 1:4, 1:4)))
  CALL bar(I_CUBE(:,1,1:4)) ! CHECK: bar(ImplicitArrayPackExpr(i_cube(:, 1, 1:4)))
  CALL foo(I_CUBE(:,1,1)) ! CHECK: foo(i_cube(:, 1, 1))
  CALL foo(I_CUBE(1,:,1)) ! CHECK: foo(ImplicitArrayPackExpr(i_cube(1, :, 1)))

  I_MAT(:) = 0 ! expected-error {{array subscript must have 2 subscript expressions}}

ENDPROGRAM arrtest
