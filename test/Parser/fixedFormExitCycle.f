* RUN: %flang -fsyntax-only %s
      PROGRAM test
      doi=1,10
      exit
      enddo
      outer:doi=1,10
        inner:doj=1,10
          exitouter
        enddoinner
        cycleouter
      enddoouter
      END
