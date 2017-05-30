TODO LIST
=========

This document describes various tasks and features that are wanted or
in-progress.

General Cleanup/Refactoring Tasks
---------------------------------

### Finish DBusv1 API and move `wpanctl` to use it ###

The new DBusv1 API is all but complete, but we still need to move a
few methods in `wpanctl` over to use the new API. We also need to add
the DBusv1 API equivalent for the `GetInterfaces` method, which is
currently only in the V0 API.



Security Tasks
--------------

Since WPAN Tunnel Driver communicates directly with the dangerous
uncontrolled real-world, additional hardening is warranted. The
following tasks are important for hardening `wpantund` from malicious foes:

 *  Investigate ways to harden the process by Limiting kernel attackable
    surface-area by using a syscall filter like [`libseccomp`](https://github.com/seccomp/libseccomp).
 *  Full security audit of code paths which directly interpret data
    from the NCP. Should be performed after the refactoring described
    above.

Wanted Features/Tasks
---------------------

 *  Provide more behavioral logic in the `NCPInstanceBase` class.
