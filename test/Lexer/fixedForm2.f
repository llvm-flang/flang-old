* RUN: %flang -fsyntax-only %s
* RUN: %flang -fsyntax-only -ast-print %s 2>&1 | %file_check %s
* an extract from chemm.f
      SUBROUTINE FOO(M, N, ALPHA, BETA)
      REAL M, N, ALPHA, BETA
      PARAMETER (ZERO = 0.0)
      PARAMETER (ONE = 1.0)
*
*     Quick return if possible.
*
      IF ((M.EQ.0) .OR. (N.EQ.0) .OR.
     +    ((ALPHA.EQ.ZERO).AND. (BETA.EQ.ONE))) RETURN
*
*     And when  alpha.eq.zero.
*
      M = N
     ++1.0
* CHECK: m = (n+1)
      RETURN
      END
