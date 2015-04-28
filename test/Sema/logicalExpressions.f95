! RUN: %flang -fsyntax-only -verify < %s
PROGRAM logicalexpressions
  IMPLICIT NONE
  LOGICAL L

  L = .true.
  L = L .AND. L
  L = L .OR. .TRUE.
  L = L .EQV. L
  L = L .NEQV. .FALSE.

  L = .NOT. L
  L = .NOT. .TRUE.

  L = .FALSE. .OR. (.TRUE. .AND. .FALSE.)

  L = L .AND. 2 ! expected-error {{invalid operands to a logical binary expression ('logical' and 'integer')}}
  L = L .OR. 'HELLO' ! expected-error {{invalid operands to a logical binary expression ('logical' and 'character')}}
  L = 3.0 .EQV. 2.0d0 ! expected-error {{invalid operands to a logical binary expression ('real' and 'double precision')}}
  L = L .NEQV. (1.0,2.0) ! expected-error {{invalid operands to a logical binary expression ('logical' and 'complex')}}

  L = .NOT. 2 ! expected-error {{invalid operand to a logical unary expression ('integer')}}
  L = .NOT. (0,0) ! expected-error {{invalid operand to a logical unary expression ('complex')}}

END PROGRAM
