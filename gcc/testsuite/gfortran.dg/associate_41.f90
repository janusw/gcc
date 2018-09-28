! { dg-do compile }
!
! PR 87460: [F03] accepts-invalid bug with ASSOCIATE and array argument
!
! Contributed by Janus Weil <janus@gcc.gnu.org>

program p

   implicit none
   integer, dimension(1:9) :: nine

   call sub(nine(1:6))    ! { dg-error "Actual argument contains too few elements" }
   call sub(nine(1:9:2))  ! { dg-error "Actual argument contains too few elements" }

   associate(six => nine(1:6))
      call sub(six)       ! { dg-error "Actual argument contains too few elements" }
   end associate

   associate(five => nine(1:9:2))
      call sub(five)      ! { dg-error "Actual argument contains too few elements" }
   end associate

contains

   subroutine sub(arg)
      integer, dimension(1:9) :: arg
   end subroutine

end
