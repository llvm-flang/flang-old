! RUN: %flang -emit-llvm -o - %s
PROGRAM test

  WRITE (*,*) 'Hello world!', 2, 3.5, .true., (1.0, 2.0)
  PRINT *, 'Hello world!'

END PROGRAM
