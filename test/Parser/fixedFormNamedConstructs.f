* RUN: %flang -fsyntax-only %s
      PROGRAM test
      c:DOI=1,10
      ENDDOc
      b:DOWHILE(.false.)
      ENDDOb
      a:IF(.true.)THEN
      ELSEIF(.false.)THENa
      ELSEa
      ENDIFa
      END
