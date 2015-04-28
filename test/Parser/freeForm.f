! RUN: %flang -ffree-form -fsyntax-only -verify < %s

program test
  integer ii, i
  i = i i ! expected-error {{expected line break or ';' at end of statement}}
end program
