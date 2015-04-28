! RUN: %flang -fsyntax-only -verify < %s
PROGRAM inc
INCLUDE 22 ! expected-error {{expected 'FILENAME'}}
INCLUDE '' ! expected-error {{empty filename}}
INCLUDE 'thisFileDoesntExist.f95' ! expected-error {{'thisFileDoesntExist.f95' file not found}}
END PROGRAM inc
