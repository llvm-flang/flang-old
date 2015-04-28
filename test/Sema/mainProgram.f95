! RUN: %flang -fsyntax-only -verify < %s
PROGRAM Bees

END PROGRAM Beads ! expected-error {{expected program name 'bees'}}
