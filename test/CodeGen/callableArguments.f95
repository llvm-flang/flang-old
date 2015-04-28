! RUN: %flang -emit-llvm -o - %s | %file_check %s

SUBROUTINE SUB(F, G) ! CHECK: void (i32*)* %f, i32 (float*)* %g
  EXTERNAL F
  LOGICAL G
  EXTERNAL G

  CALL F(2) ! CHECK: call void %f(i32*
  IF(G(3.0)) THEN ! CHECK: call i32 %g(float*
  END IF
END

SUBROUTINE F(I)
END

LOGICAL FUNCTION FOO(R)
  Foo = .true.
END

PROGRAM test
  CALL SUB(F, FOO) ! CHECK: call void @sub_(void (i32*)* @f_, i32 (float*)* @foo_)
END
