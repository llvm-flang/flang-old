! RUN: %flang -fsyntax-only -verify < %s
PROGRAM intrintest
  INTRINSIC INT, MOD
  INTRINSIC :: ABS

  INTRINSIC REAL DBLE ! expected-error {{expected ','}}
  INTRINSIC 'LLE' ! expected-error {{expected identifier}}

END PROGRAM
