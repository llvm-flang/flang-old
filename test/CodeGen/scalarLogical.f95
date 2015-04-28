! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  LOGICAL L            ! CHECK: alloca i32
  INTEGER I

  L = L                ! CHECK: load i32*
  L = .NOT. L          ! CHECK: xor i32

  L = .TRUE. .EQV. L   ! CHECK: icmp eq i1 true
  L = .FALSE. .NEQV. L ! CHECK: icmp ne i1 false

  I = 5

  L = I .LT. 10 .AND. I .GT. 1
  CONTINUE ! CHECK: icmp slt i32
  CONTINUE ! CHECK: br i1
  CONTINUE ! CHECK: load i32*
  CONTINUE ! CHECK: icmp sgt i32
  CONTINUE ! CHECK: br i1
  CONTINUE ! CHECK: br label
  CONTINUE ! CHECK: br label
  CONTINUE ! CHECK: phi i1

  L = 0 .EQ. I .OR. 1 .EQ. I
  CONTINUE ! CHECK: icmp eq i32 0
  CONTINUE ! CHECK: br i1
  CONTINUE ! CHECK: load i32*
  CONTINUE ! CHECK: icmp eq i32 1
  CONTINUE ! CHECK: br i1
  CONTINUE ! CHECK: br label
  CONTINUE ! CHECK: br label
  CONTINUE ! CHECK: phi i1

END PROGRAM
