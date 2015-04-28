! RUN: %flang -emit-llvm -o - %s | %file_check %s
PROGRAM test
  X(I) = I + 1
  COMPLEX A
  COMPLEX J
  J(A) = A * (1.0,1.0)
  CHARACTER*(5) ABC
  ABC() = 'VOLBE'

  REAL XVAL
  COMPLEX JVAL

  XVAL = X(2) ! CHECK: store float 3
  JVAL = J((1.0,2.0))

  PRINT *, ABC()

END PROGRAM test
