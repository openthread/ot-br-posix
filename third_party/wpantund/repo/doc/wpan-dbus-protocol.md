wpantund DBus Protocol v1
=========================

This document outlines the DBus protocol used to communicate
with `wpantund` to manage a low-power wireless network interface.

It is a work in progress.

daemon id: `org.wpantund`

# Interface: `org.wpantund`

## Path `/org/wpantund/`

### Command: `GetInterfaces`
### Command: `GetVersion`
### Signal: `InterfaceAdded`
### Signal: `InterfaceRemoved`

## Path `/org/wpantund/<iface-name>`

### Signal: `NetScanBeacon`
### Signal: `NetScanComplete`

### Command: `NetScanStart`
### Command: `NetScanStop`

### Command: `Status`
### Command: `Reset`

### Command: `BeginLowPower`
### Command: `HostDidWake`

### Command: `Attach`
### Command: `Join`
### Command: `Form`
### Command: `Leave`

### Command: `ConfigGateway`
### Command: `DataPoll`

## Path `/org/wpantund/<iface-name>/Properties/<property-name>`

### Signal: "Changed"
### Command: "Set"
### Command: "Get"

# Interface: `com.nestlabs.wpantund.internal`

## Path `/org/wpantund/<iface-name>`

### Command: `PermitJoin`
### Command: `BeginNetWake`



-------------------------------------------------------------------------------

# Configuration Properties

## `Config:NCP:SocketPath`
## `Config:NCP:SocketBaud`
## `Config:NCP:DriverName`
## `Config:NCP:HardResetPath`
## `Config:NCP:PowerPath`
## `Config:NCP:ReliabilityLayer`
## `Config:NCP:FirmwareCheckCommand`
## `Config:NCP:FirmwareUpgradeCommand`
## `Config:TUN:InterfaceName`
## `Config:Daemon:PIDFile`
## `Config:Daemon:PrivDropToUser`
## `Config:Daemon:Chroot`

## `Daemon:Version`
## `Daemon:Enabled`
## `Daemon:SyslogMask`
## `Daemon:TerminateOnFault`


## `Daemon:ReadyForHostSleep`
Read only. Set to `true` if `wpantund` is busy. Set to `false` if it is idle.
Primarily used for determining if it is a good opportunity to attempt to
put the host into a low-power state.

## `Daemon:AutoAssociateAfterReset` (Was AutoResume)
If the NCP persistently stores network association and the NCP
comes up in the `offline:commissioned` state, this option being set
will cause `wpantund` to automatiaclly attempt to associate.

## `Daemon:AutoFirmwareUpdate`
## `Daemon:AutoDeepSleep`

## `NCP:Version`
## `NCP:State`
## `NCP:HardwareAddress`
## `NCP:Channel`
## `NCP:TXPower`
## `NCP:TXPowerLimit`
## `NCP:CCAThreshold`
## `NCP:DefaultChannelMask`
## `NCP:SleepyPollInterval`

## `Network:Name`
## `Network:XPANID`
## `Network:PANID`
## `Network:NodeType`
## `Network:Key`
## `Network:KeyIndex`
## `Network:IsAssociated`

## `IPv6:LinkLocalAddress`
## `IPv6:MeshLocalAddress`
## `IPv6:AllAddresses`









# NCP States

## "uninitialized"                  (same)
## "uninitialized:fault"            (same)
## "uninitialized:upgrading"        (same)
## "offline:deep-sleep"             (was "deep-sleep")
## "offline"                        (was "disconnected")
## "offline:commissioned"           (was "saved")
## "associating"                    (was "joining")
## "associating:credentials-needed" (was "credentials-needed")
## "associated"                     (was "joined")
## "associated:no-parent"           (was "joined-no-parent")
## "associated:netwake-sleeping"    (was "joined-lurking")
## "associated:netwake-waking"      (was "net-wake")
