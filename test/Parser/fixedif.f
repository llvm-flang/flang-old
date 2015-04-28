* RUN: %flang -fsyntax-only %s
* RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
      PROGRAM iftest
* an implicit integer declaration.
* CHECK: ifatal = 0
      IFATAL = 0
* an if statement
      IF(.FALSE.)IFATAL=0
      IF(.true.)THEN
        IFATAL=0
      ELSEIF(.false.)THEN
        IFATAL=1
      ENDIF
      END
