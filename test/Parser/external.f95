! RUN: %flang -fsyntax-only -verify < %s
PROGRAM exttest
  EXTERNAL FUNC, FUNC2

  EXTERNAL X Y ! expected-error {{expected ','}}
  EXTERNAL 'ABS' ! expected-error {{expected identifier}}
  EXTERNAL ZZ, &
    MM, ! expected-error {{expected identifier}}

  ! FIXME: move error location to after vv
  EXTERNAL VV &
    NN ! expected-error {{expected ','}}

END PROGRAM
