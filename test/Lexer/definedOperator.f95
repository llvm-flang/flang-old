! RUN: %flang -fsyntax-only -verify < %s
PROGRAM definedOperators
  .OP ! expected-error {{defined operator missing end '.'}}
END PROGRAM definedOperators
