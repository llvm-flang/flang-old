! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM gototest
    INTEGER I

10  I = 0
20  GOTO (10,30) I ! CHECK: switch i32

30  I = 1

END PROGRAM
