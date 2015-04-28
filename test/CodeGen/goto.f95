! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM gototest

1000 CONTINUE   ! CHECK: ; <label>:2
     GOTO 1000  ! CHECK: br label %2

     GOTO 2000
2000 CONTINUE

END
