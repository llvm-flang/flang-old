! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM iftest
  INTEGER I
  LOGICAL L

  IF(.true.) I = I ! CHECK: br i1 true
  L = .false.      ! CHECK: br label

  IF(L) THEN       ! CHECK: br i1
    I = I * 2      ! CHECK: br label
  ELSE
    I = I + 2      ! CHECK: br label
  END IF

  IF(I .LE. 0) THEN       ! CHECK: icmp sle
    I = I + 2             ! CHECK: br i1
  ELSE IF(I .GT. 10) THEN ! CHECK: icmp sgt
    I = I / 4             ! CHECK: br i1
  ELSE
    I = -I
  END IF

END PROGRAM
