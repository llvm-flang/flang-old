      program hello
C RUN: %flang -fsyntax-only %s
      integer i
      logical l
      if(l . and. l) i = 2
      if(l . a nd . i . e q. 0) then
      end if
      end
