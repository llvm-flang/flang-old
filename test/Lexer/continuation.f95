! RUN: %flang -fsyntax-only -verify < %s
& ! expected-error {{continuation character used out of context}}
PROGRAM continuations
  & ! expected-error {{continuation character used out of context}}
END PROGRAM continuations
