C RUN: %flang -fsyntax-only %s
C RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s

       SUBROUTINE FOO
       INTEGER SAVEALPHA
       SAVESAVEALPHA
C CHECK: savealpha = 1
       SAVEALPHA=1
C CHECK-NEXT: savebeta = real(1)
       SAVEBETA=1
C CHECK-NEXT: savegamma = real(2)
       SAVEGAMMA=2
       END
