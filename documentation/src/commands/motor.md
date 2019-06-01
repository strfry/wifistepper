---
layout: default
title: Motor Control
parent: Commands
nav_order: 1
---

# Motor Commands
{: .no_toc }
These commands effect the operation of the motor in some way. Typically (unless otherwise mentioned), they are represented by data structures that are placed in a command queue (see `queue` parameter). They occupy a length in bytes and will only be added to the queue if there is sufficient room. A command is evaluated when it is at the head of the **Execution Queue** (Queue *0*). Once a command is evaluated, it is popped off the Execution Queue and the next command is then evaluated. If the Execution Queue queue is empty, the queue remains idle and the no new commands are evaluated until they are placed in the queue.

Some commands have preconditions prior to execution (ex. *GoTo* requires motor must not be in a **BUSY** state). If the preconditions are not met, the command is not evaluated until they are met. In some circumstances it is possible to enter a dead-locked state. For example if a *Run* command is executed (which maintains a constant speed) followed by a *StepClock* command (which has the precondition that the motor is stopped), the Execution Queue will enter a dead-locked state. The system will wait patiently for a precondition that will never be met. Issuing an *EStop* or a *EmptyQueue* command will exit this dead-locked state. It is up to the programmer to understand the preconditions and never issue commands that would dead-lock the Execution Queue.

The **EStop (Emergency Stop)** is special in that it is not associated with a queue and does not have any preconditions. When issued, this command immediately stops evaluation of the Execution Queue to halt the motor according to the parameters given. Once evaluated, the Execution Queue is cleared and will remain idle until new commands are issued.

For more information on the Execution Queue and the Command Queues in general, see the [Command Queue](/command-queue.html) page.

Each command has an associated `target` parameter (default 0). This parameter is used when **Daisy Chaining** is enabled to route the command to the target motor over the high speed serial bus. See [Daisy Chain](/daisy-chain.html) for more information.


## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---
# Registers
## Pos

The **Pos** register is updated whenever the motor shaft moves and keeps track of the current microstep. It is a 22-bit signed integer ranging from (-2^21 to +2^21 - 1) and will overflow/underflow without errors. On power cycle, this register is reset to zero (Home position).

The Pos register is readable through the motor *state* interface. Set this register using the *[SetPos](#setpos)* and *[ResetPos](#resetpos)* commands.

## Mark

The **Mark** register is used to mark a certain position of the motor shaft.

The Mark register is readable through the motor *state* interface. Utilize this register with the *[SetMark](#setmark)* and *[ResetPos](#gomark)* commands. Additionally, *[GoUntil](#gountil)* and *[ReleaseSW](#releasesw)* commands may act on this register.

## Busy

The **BUSY** flag is used to synchronize motor motions. Depending on the command, the BUSY flag will be asserted until the shaft has reached some speed or position. Read this value through the motor *state* interface or use the *[WaitBusy](#waitbusy)* command to delay Command Queue execution.

---
# Commands
## EStop (Emergency Stop)
This command is intended to safe the motor during an emergency stop operation or to shut down the motor down when it is no longer needed. This command is similar to the [Stop](#stop) command except it is evaluated immediately when issued and clears the Execution Queue upon completion.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| hiz | Bool | Disable the motor bridges (put in High-Impedance state) when stopped. When false, motor bridges will remain active. | true |
| soft | Bool | Use deceleration phase to stop motor. When false, motor will perform a **Hard Stop** (stops motor immediately). | true |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |

- **Preconditions:** None, this command will be evaluated immediately when issued.
- **Effect on BUSY flag:** Asserts BUSY flag until motor is stopped.
- **Bytes allocated in Queue:** This command does not allocate any bytes and cannot be assigned to a queue.
- **Side Effects:** The Execution Queue is emptied and will remain idle until new commands are issued.

---
## GoHome
This command is equivalent to [GoTo(pos=**0**)](#goto) with the direction being the minimum path to the Home position.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to | 0 |

- **Preconditions:** The BUSY flag must not be asserted.
- **Effect on BUSY flag:** Asserts BUSY flag until home position is reached.
- **Bytes allocated in Queue:** 5 Bytes.
- **Side Effects:** None.

---
## GoMark
This command is equivalent to [GoTo(pos=**MARK**)](#goto) with the direction being the minimum path to the configured [MARK](#mark)

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The BUSY flag must not be asserted.
- **Effect on BUSY flag:** Asserts BUSY flag until mark position is reached.
- **Bytes allocated in Queue:** 5 Bytes.
- **Side Effects:** None.

---
## GoTo
Rotates the motor shaft to the specified position in microsteps (see [microstep](#configuration) config) relative to Home position (zero position) and holds with active bridges. Observes acceleration, deceleration, and max speed settings and manages motor power according to KTvals in the appropriate phase of operation. When `dir` parameter is specified, the motor will only rotate according to that parameter. If `pos` is behind the current position, the shaft will spin until the [Pos](#pos) register overflows/underflows and then reaches desired position.

The Quick Start guide utilizes this command in the Servo Control profile to rotate and hold the position of the shaft.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| pos | Int | The position to rotate the shaft (in microsteps) relative to the [Home](#pos) position. Can be positive or negative. | (required) |
| dir | Enum(forward, reverse) | The direction to spin | *auto* |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The BUSY flag must not be asserted.
- **Effect on BUSY flag:** Asserts BUSY flag until specified position is reached.
- **Bytes allocated in Queue:** 11 Bytes.
- **Side Effects:** None.

---
## GoUntil
Rotates the motor shaft at the specified steps per second until the external switch (**SW** pin) is closed (shorted to ground). When switch is closed, the motor performs a *[Stop(hiz=**false**,soft=**true**)](#stop)* command and the specified `action` is performed. Observes acceleration, deceleration, min speed, and max speed limits. Motor power is managed according to KTvals in the appropriate phase of operation. 

The specified `action` param must be one of the following:
- **RESET:** [Pos](#pos) register is set to zero (Home position)
- **COPYMARK:** The current microstep position is copied into the [Mark](#mark) register.

Note: This command is useful during homing and can be coupled with *[WaitBusy](#waitbusy)* then *[ReleaseSW](#releasesw)* for more accuracy.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| action | Enum(reset, copymark) |  | (required) |
| dir | Enum(forward, reverse) | The direction to spin | (required) |
| stepss | Float | The number of full steps per second to rotate (doesn't depend on microstep config). This is a decimal parameter so specifying fractional steps per second is allowed. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** Asserts BUSY flag until the switch is closed.
- **Bytes allocated in Queue:** 11 Bytes.
- **Side Effects:** None.

---
## Move
Moves the shaft the given number of microsteps in the given direction from the current position.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| dir | Enum(forward, reverse) | The direction to spin | (required) |
| microsteps | Int | Number of microsteps to move | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The motor is stopped.
- **Effect on BUSY flag:** Asserts BUSY flag until number of microsteps has been reached.
- **Bytes allocated in Queue:** 10 Bytes.
- **Side Effects:** None.

---
## ReleaseSW
Rotates the motor shaft at the minimum speed configuration setting while the external switch (**SW** pin) is closed (shorted to ground). When switch is opened, the motor performs a *[Stop(hiz=**false**,soft=**false**)](#stop)* (hard stop) command and the specified `action` is performed. Motor power is managed according to KTvals. 

The specified `action` param must be one of the following:
- **RESET:** [Pos](#pos) register is set to zero (Home position)
- **COPYMARK:** The current microstep position is copied into the [Mark](#mark) register.

Note: This command is intended for use during a homing operation. See [GoUntil](#gountil) for more information.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| action | Enum(reset, copymark) |  | (required) |
| dir | Enum(forward, reverse) | The direction to spin | (required) |
| stepss | Float | The number of full steps per second to rotate (doesn't depend on microstep config). This is a decimal parameter so specifying fractional steps per second is allowed. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** Asserts BUSY flag until the switch is closed.
- **Bytes allocated in Queue:** 7 Bytes.
- **Side Effects:** None.

---
## ResetPos
Set the [Pos](#pos) register to zero. This has the effect of making the current shaft position the Home position.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 5 Bytes.
- **Side Effects:** The [Pos](#pos) register is set to zero.

---
## Run
Rotates the motor shaft at the specified steps per second. Observes acceleration, deceleration, and max speed limits. Motor power is managed according to KTvals in the appropriate phase of operation. 

The Quick Start guide utilizes this command in the Speed Control profile.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| dir | Enum(forward, reverse) | The direction to spin | (required) |
| stepss | Float | The number of full steps per second to rotate (doesn't depend on microstep config). This is a decimal parameter so specifying fractional steps per second is allowed. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** Asserts BUSY flag until specified speed is reached.
- **Bytes allocated in Queue:** 10 Bytes.
- **Side Effects:** None.

---
## RunQueue
When at the head of the Execution Queue, this command will copy the `targetqueue` into it's current position at the head, shifting all other queued commands down. If there is not enough space in the Execution Queue for `targetqueue` + rest of queue, an error status is set and this command is skipped. *RunQueue* has no effect on the motor.

You can use this command to enable looping and/or group complex motions into queues for easy execution. 

**Note:** This is *not* a queue management command, it can be added to any queue at any time and is only evaluated when it is at the head of the *Execution Queue*.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| targetqueue | Int | The queue to copy into the Execution Queue (cannot be 0) | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 6 Bytes.
- **Side Effects:** The length of the *Execution Queue* is extended by the number of bytes in the `targetqueue` queue.

---
## SetConfig
Apply the configuration settings to the motor. This command is added to the specified `queue` and is only applied when the command is evaluated (and preconditions are met).

The `config` parameter is a JSON object with keys matching the configuration entries in the [Configuration](#configuration) section. All keys are optional and if omitted, the configuration value is left unchanged.

**Example:**
```json
{"mode":"voltage","stepsize":16}
```
In this example, only the mode is changed to voltage and stepsize is set to 16, all other configuration values are left unchanged.

**Important Note:** If this command is embedded in a larger JSON object (for example when using [MQTT](/interfaces/mqtt.html)), `config` is treated as a raw string and must be escaped (ie. quotes " must be replaced by \\")

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| config | String | JSON-formatted configuration string. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** Motor must be stopped.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 6 Bytes + length of `config`.
- **Side Effects:** None.

---
## SetMark
Set the current shaft position to `pos` in the [Mark](#mark) register.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| pos | Int | The position to write to the [Mark](#pos) register. Can be positive or negative. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 9 Bytes.
- **Side Effects:** The [Mark](#pos) register is set to the `pos` param.

---
## SetPos
Set the current shaft position to `pos` in the [Pos](#pos) register.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| pos | Int | The position to write to the [Pos](#pos) register. Can be positive or negative. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 9 Bytes.
- **Side Effects:** The [Pos](#pos) register is set to the `pos` param.

---
## StepClock
Step Clock mode uses square waves on the **STEP** input pin to advance the shaft. One cycle of 3.3 volts to ground will advance the shaft one microstep in the direction of the `dir` param.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| dir | Enum(forward, reverse) | The direction to rotate the shaft. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The motor is stopped.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 6 Bytes.
- **Side Effects:** The motor will be in Step Clock mode until further motion commands exit Step Clock mode.

---
## Stop
Stops the motor (zero shaft rotation) according to the parameters given.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| hiz | Bool | Disable the motor bridges (put in High-Impedance state) when stopped. When false, motor bridges will remain active. | true |
| soft | Bool | Use deceleration phase to stop motor. When false, motor will perform a **Hard Stop** (stops motor immediately). | true |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** None.
- **Effect on BUSY flag:** Asserts BUSY flag until motor is stopped.
- **Bytes allocated in Queue:** 7 Bytes.
- **Side Effects:** None.

---
## WaitBusy
Because of the precondition requirement, *WaitBusy* will wait until the motor BUSY flag is not asserted before completing. Use this command to ensure the previous command has completed according to its *Effect on BUSY flag* prior to the next command evaluation. *WaitBusy* has no effect on the motor.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The BUSY flag is not asserted.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 5 Bytes.
- **Side Effects:** None.

---
## WaitMS
On initial evaluation of the precondition, a timer is started. This command will not complete until the timer value has reached the `ms` param. The timer is not attached to an interrupt and as such the `ms` param is a minimum bound. Other network services may cause millisecond-level delays. *WaitMS* has no effect on the motor.

You can use this command with other *Wait\** commands for finer timing control. For example by adding \[*[GoTo](#goto)*, *[WaitRunning](#waitrunning)*, *WaitMS*\], the evaluation will only complete once the motor has reached the *GoTo* location **and** delayed by the given milliseconds.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| ms | Int | The amount of milliseconds (after initial evaluation of the precondition) to wait before preceding to the next command. | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The milliseconds have elapsed since initial precondition check.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 13 Bytes.
- **Side Effects:** None.

---
## WaitRunning
This command will not be evaluated until the motor is not running (stopped) in either active bridge state or high-impedance state. Use this command to time the previous command with the next command evaluation. *WaitRunning* has no effect on the motor.

| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The motor is stopped.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 5 Bytes.
- **Side Effects:** None.

---
## WaitSwitch
This command will not complete until the external switch (**SW** pin) has entered the state given by the `state` param. *WaitSwitch* has no effect on the motor.

- **CLOSED:** The *SW* pin is shorted to ground
- **OPEN:** The *SW* pin is connected to 3.3v supply or left floating (open circuit).

Note: There is a pullup-resistor on the *SW* pin, so an open circuit is electrically equivalent to 3.3 volts.


| Parameter | Type | Description | Default |
|:--|:--|:--|:--:|
| state | Enum(closed, open) | The switch state to check against | (required) |
| *target* | Int | When [Daisy Chain](/daisy-chain.html) enabled, issue this command to the target motor. | 0 |
| *queue* | Int | The [Queue](/command-queue.html) to add this command to. | 0 |

- **Preconditions:** The switch has entered the given state.
- **Effect on BUSY flag:** None.
- **Bytes allocated in Queue:** 6 Bytes.
- **Side Effects:** None.

---
# Configuration

## Common Config 

### Control Mode

Specifies which mode to operate the motor under. *Current Mode* utilizes the current sense resistors to detect and maintain peak current. Under *Voltage Mode*, current sense resistors are not necessary and may be bypassed by shorting the jumpers on the bottom side.  

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **mode** | Enum(current, voltage) | - | - | *current* |

---
### Microstep Size

Utilize microstep waveforms. Under *Current Mode* 1 - 16 microstep is available. With *Voltage Mode* down to 128 is available.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **stepsize** | Int | - | 1,2,4,8,16\[,32,64,128\] | 16 |

---
### Over Current Detection Threshold

The voltage level over the commanded voltage (measured at the driver MOSFETS) is considered an over voltage event.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **ocd** | Float | mV | 0 - 1000 | 500 |

---
### Over Current Shutdown Enabled

When enabled, an Over Current flag (triggered by the [Over Current Detection Threshold](#over-current-detection-threshold)) with shut down the bridges to prevent damage.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **ocdshutdown** | Bool | - | - | true |

---
### Maximum Speed

Limit the motor shaft to this maximum speed. Any motion command over this speed will be limited to this speed.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **maxspeed** | Float | steps/s | 15.25 - 15610 | 10000 |

---
### Minimum Speed

Limit the minimum motor shaft to this speed. Any motion command under this speed will be forced up to this speed.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **minspeed** | Float | steps/s | 0 - 976.3 | 0 |

---
### Acceleration

Defines how much acceleration to speed up the motor when commanding higher speeds.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **accel** | Float | steps/s^2 | 14.55 - 59590 | 1000 |

---
### Deceleration

Defines how much deceleration to slow down the motor when commanding lower speeds or stop.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **decel** | Float | steps/s^2 | 14.55 - 59590 | 1000 |

---
### Full Step Speed Changeover

Defines at which speed to change over from microstepping to full steps. If set too low, there will be a noticable jerk during transition.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **fsspeed** | Float | steps/s | 7.63 - 15625 | 2000 |

---
### Full Step Boost Enabled

When set, the amplitude of a full step is the peak of voltage sinewave during microstepping, improving output current and motor torque. However, a discontinuous jump in current is likely.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **fsboost** | Bool | - | - | false |

---
### Reverse Wires Enabled

Depending on how the wires are connected, the motor may rotate backwards to what is commanded. When enabled, the wires are 'virtually' reversed through software. Alternatively to this setting, you may reverse polarity of *one* of the A or B motor wire connections.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **reverse** | Bool | - | - | false |

---
### EEPROM Save

Save the configuration to the EEPROM. Upon power cycle, the configuration state will be applied.

**Note:** This is not a configuration entry. This is a directive to write the active configuration to EEPROM once the configuration has been applied. 

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|:--|
| **save** | Bool | - | - | false |

---
## Current Mode Only Config

The following configuration entries only apply when *Current Mode* is a active. The may be written at any time.

**Note:** These entries are prefixed with **cm_\***.

### KT Hold (Current Mode)

The peak current to drive the stepper motor when commanded to hold a position (with no rotation). While this setting accepts values up to 14 amps, it is recommended not to exceed 7 amps as holding a shaft position can cause current DC states.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_kthold** | Float | Amps | 0 - 14 | 2.5 |

---
### KT Run (Current Mode)

The peak current to drive the stepper motor when commanded to rotate at a constant speed (no acceleration or deceleration).

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_ktrun** | Float | Amps | 0 - 14 | 2.5 |

---
### KT Acceleration (Current Mode)

The peak current to drive the stepper motor when commanded to accelerate rotational speed.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_ktaccel** | Float | Amps | 0 - 14 | 2.5 |

---
### KT Deceleration (Current Mode)

The peak current to drive the stepper motor when commanded to decelerate rotational speed.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_ktdecel** | Float | Amps | 0 - 14 | 2.5 |

---
### Switch Period (Current Mode)

The switching period of the current control algorithm.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_switchperiod** | Float | microseconds | 4 - 124 | 44 |

---
### Current Prediction Compensation Enabled (Current Mode)

When enabled, the current control algorithm remembers offset adjustments and attempts to predict and minimize them in the future.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_predict** | Bool | bool | - | true |

---
### Minimum On Time (Current Mode)

The minimum gate on time of the current control algorithm. This setting impacts how the waveform is generated to control the current flowing through the motor coils.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_minon** | Float | microseconds | 0.5 - 64 | 21 |

---
### Minimum Off Time (Current Mode)

The minimum gate off time of the current control algorithm. This setting impacts how the waveform is generated to control the current flowing through the motor coils.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| Minimum Off Time | **cm_minoff** | Float | microseconds | 0.5 - 64 | 21 |

---
### Fast Decay Time (Current Mode)

The maximum fast decay time of the current control algorithm. This setting impacts how the waveform is generated to control the current flowing through the motor coils.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_fastoff** | Float | microseconds | 2 - 32 | 4 |

---
### Fast Decay Step Time (Current Mode)

The maximum fall step time of the current control algorithm. This setting impacts how the waveform is generated to control the current flowing through the motor coils.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **cm_faststep** | Float | microseconds | 2 - 32 | 20 |

---
## Voltage Mode Only Config

The following configuration entries only apply when *Voltage Mode* is a active. The may be written at any time.

**Note:** These entries are prefixed with **vm_\***.

### KT Hold (Voltage Mode)

The percentage of input voltage to drive the stepper motor when commanded to hold a position (with no rotation).

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_kthold** | Float | % of Vin | 0 - 100 | 15 |

---
### KT Run (Voltage Mode)

The percentage of input voltage to drive the stepper motor when commanded to rotate at a constant speed (with no acceleration or deceleration).

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_ktrun** | Float | % of Vin | 0 - 100 | 15 |

---
### KT Acceleration (Voltage Mode)

The percentage of input voltage to drive the stepper motor when commanded to accelerate rotational speed.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_ktaccel** | Float | % of Vin | 0 - 100 | 15 |

---
### KT Deceleration (Voltage Mode)

The percentage of input voltage to drive the stepper motor when commanded to decelerate rotational speed.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_ktdecel** | Float | % of Vin | 0 - 100 | 15 |

---
### PWM Frequency (Voltage Mode)

The PWM Frequency to drive the motor at when in *Voltage Control* mode.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_pwmfreq** | Float | kHz | 2.8 - 62.5 | 23.4 |

---
### Stall Threshold (Voltage Mode)

What voltage over the commanded voltage (measured at the driver MOSFETS) is considered a motor stall. During a stall, the back EMF can be detected and the stall flag asserted.

**Note:** This feature is only available in *Voltage Mode*

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_stall** | Float | mV | 31.25 - 1000 | 750 |

---
### Voltage Supply Compensation Enabled (Voltage Mode)

Utilize the power supply voltage compensation feature. Certain unmanaged or less expensive power supplies will sag or swell under current transients. To prevent changes in torque if this happens, the Vin monitoring of the stepper motor driver can be utilized to increase or decrease the *KT vals* during transients.

**Note:** this will disable *Vin* input voltage measurement.

** Important Note:** this requires board modification of the JP4 and R1 pads. 
- Solder the appropriate resistor to **R1** (0805 package). The value can be found using the formula `R1 = Vin * 1091 - 1800`. Common power supply resistor values are shipped with this product (12v, 24v, 48v).
- Modify jumper **JP4** according to the accompanying figure. The left side should be cut and the right side should be bridged (shorted) with solder. This is needed to remove the *Vin* sense resistor and include the **R1** resistor in the circuitry.

![](data:image/png;base64,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)

To revert this modification, remove solder bridge on right and add solder bridge on left of **JP4**.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_volt_comp** | Bool | - | - | false |

---
### Back EMF Low Slope Compensation (Voltage Mode)

How much to adjust the voltage to compensate for Back EMF during slow speed operation on acceleration and deceleration phases.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_bemf_slopel** | Float | % | 0 - 0.4 | 0.0375 |

---
### Back EMF Slope Changeover Speed (Voltage Mode)

At which speed to change over between slow and fast speed Back EMF slope compensation.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_bemf_speedco** | Float | steps/s | 0 - 976.5 | 61.5072 |

---
### Back EMF High Slope Acceleration Compensation (Voltage Mode)

How much to adjust the voltage to compensate for Back EMF during fast speed operation on acceleration phase.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_bemf_slopehacc** | Float | % | 0 - 0.4 | 0.0615 |

---
### Back EMF High Slope Deceleration Compensation (Voltage Mode)

How much to adjust the voltage to compensate for Back EMF during fast speed operation on deceleration phase.

| Key | Type | Unit | Range | Default |
|:--|:--|:--|:--|:--:|
| **vm_bemf_slopehdec** | Float | % | 0 - 0.4 | 0.0615 |
