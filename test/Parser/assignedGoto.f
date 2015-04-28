C RUN: %flang -fsyntax-only -verify %s

       PROGRAM test
       INTEGERI,TOI
       ASSIGN100TOI
C FIXME: fixed-form comments
C expected-error@+1 {{expected 'to'}}
       ASSIGN100
        TO I = 1
100    CONTINUE
       E ND PRO GRAMt e s t

