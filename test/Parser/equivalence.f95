! RUN: %flang -verify -fsyntax-only < %s

PROGRAM equivtest
  REAL A, B
  EQUIVALENCE (A,B)
  INTEGER I, J, K, M, N, O(3)
  EQUIVALENCE (A, I), (B, K)

  EQUIVALENCE (I,J ! expected-error {{expected ')'}}
  EQUIVALENCE (M, N) (M, O(1)) ! expected-error {{expected ','}}

END
