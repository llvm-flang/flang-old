* RUN: %flang -fsyntax-only %s
* RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s

      PROGRAM D
* CHECK: endd = 0
      INTEGER ENDD
      ENDD = 0
      END D
