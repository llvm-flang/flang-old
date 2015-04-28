* RUN: %flang -fsyntax-only %s
* RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
      LOGICALFUNCTIONFOO()
* CHECK: foo = false
        FOO = .false.
        RETURN
      END

      IN T EGE R(Kind=8) F U N CTIONB A R 2()
      END

      LOGICAL(2)FUNCTIONBAR()
C CHECK: bar = logical(true,Kind=2)
        BAR = .true.
        RETURN
      END
