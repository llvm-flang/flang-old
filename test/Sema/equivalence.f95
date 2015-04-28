! RUN: %flang -verify -fsyntax-only < %s

SUBROUTINE equivtest(JJ) ! expected-note {{'jj' is an argument defined here}}
  REAL A, B
  EQUIVALENCE (A,B) ! expected-note {{an identical association was already created here}}
  INTEGER I, J, JJ, K, M, N, O(3), I_ARR(3,3)
  CHARACTER*10 STR, STR_ARR(10)
  INTEGER(Kind=8) FOO
  PARAMETER(II = 111) ! expected-note {{'ii' is a parameter constant defined here}}
  INTEGER III ! expected-note@+1 {{an identical association was already created here}}
  EQUIVALENCE (A, I), (B, K) ! expected-note {{an identical association was already created here}}

  REAL AA ! expected-note@+2 {{previous memory offset was defined here}}
  REAL BB ! expected-note@+1 {{previous memory offset was defined here}}
  EQUIVALENCE (I, I_ARR) ! expected-note {{an identical association was already created here}}
  EQUIVALENCE (STR(2:), STR_ARR)

  EQUIVALENCE (I, O(1)) ! expected-note {{an identical association was already created here}}
  EQUIVALENCE (N, O(I)) ! expected-error {{statement requires a constant expression}}

  EQUIVALENCE (II, I) ! expected-error {{specification statement requires a local variable}}
  EQUIVALENCE (I, jj) ! expected-error {{specification statement requires a local variable}}

  EQUIVALENCE (I, 22) ! expected-error {{specification statement requires a variable or an array element expression}}

  EQUIVALENCE (I, STR) ! expected-error {{expected an expression of integer, real, complex or logical type ('character (Len=10)' invalid)}}
  EQUIVALENCE (STR, A) ! expected-error {{expected an expression of character type ('real' invalid)}}
  EQUIVALENCE (I, FOO) ! expected-error {{expected an expression with default type kind ('integer (Kind=8)' invalid)}}

  EQUIVALENCE (A, A) ! expected-warning {{this equivalence connection uses the same object}}
  EQUIVALENCE (A, B) ! expected-warning {{redundant equivalence connection}}

  EQUIVALENCE (I, A) ! expected-warning {{redundant equivalence connection}}
  EQUIVALENCE (A, I) ! expected-warning {{redundant equivalence connection}}

  EQUIVALENCE (I, I_ARR(1,1)) ! expected-warning {{redundant equivalence connection}}
  EQUIVALENCE (I_ARR(1,1), O(1)) ! expected-warning {{redundant equivalence connection}}

  EQUIVALENCE (I, I_ARR(2,2)) ! expected-error {{conflicting memory offsets in an equivalence connection}}
  EQUIVALENCE (I_ARR(2,1), I) ! expected-error {{conflicting memory offsets in an equivalence connection}}

  LOGICAL L, L_ARR(2) ! expected-note@+1 {{previous memory offset was defined here}}
  EQUIVALENCE( L, L_ARR, L_ARR(2) ) ! expected-error {{conflicting memory offsets in an equivalence connection}}

END

SUBROUTINE foo()
  REAL A
  INTEGER I, I_ARR(3,3) ! expected-note@+1 {{an identical association was already created here}}
  EQUIVALENCE (A, I) ! expected-note {{an identical association was already created here}}
  EQUIVALENCE (I, I_ARR)
  EQUIVALENCE (I, A) ! expected-warning {{redundant equivalence connection}}
  EQUIVALENCE (A, I) ! expected-warning {{redundant equivalence connection}}
END

SUBROUTINE bar()
  INTEGER I_MAT(4,4), I_MAT2(4,4)
  EQUIVALENCE(I_MAT, I_MAT(1,2)) ! expected-error {{conflicting memory offsets in an equivalence connection}}
END

