# piservod

A user space software PWM implementation for controlling servos on the Raspberry Pi.

## Build
To build and install run:

```bash
make
sudo make install
```

## Usage
The daemon will expose a Unix domain socket at `/tmp/piservod.sock` to which you can send commands.

### Starting the daemon
```bash
sudo piservod
```

Note: `sudo` is required for:
- Real-time scheduling priority (SCHED_FIFO)
- GPIO access

### Protocol
Commands are newline-delimited text strings. All commands are case-insensitive.

#### SETUP - Configure a servo channel
```
SETUP <channel> GPIO <pin>
```

Example:
```bash
echo "SETUP 0 GPIO 5" | nc -N -U /tmp/piservod.sock
# Response: OK
```

#### ENABLE - Enable a servo channel
```
ENABLE <channel>
```

Example:
```bash
echo "ENABLE 0" | nc -N -U /tmp/piservod.sock
# Response: OK
```

#### DISABLE - Disable a servo channel
```
DISABLE <channel>
```

Example:
```bash
echo "DISABLE 0" | nc -N -U /tmp/piservod.sock
# Response: OK
```

#### SET RANGE - Set the pulse width range (in microseconds)
```
SET <channel> RANGE <min> <max>
```

Example:
```bash
echo "SET 0 RANGE 1000 2000" | nc -N -U /tmp/piservod.sock
# Response: OK
```

#### SET PULSE - Set the pulse width (in microseconds)
```
SET <channel> PULSE <value>
```

Example:
```bash
echo "SET 0 PULSE 1500" | nc -N -U /tmp/piservod.sock
# Response: OK
```

#### GET RANGE - Query the pulse width range
```
GET <channel> RANGE
```

Example:
```bash
echo "GET 0 RANGE" | nc -N -U /tmp/piservod.sock
# Response: RANGE 1000 2000
```

#### GET PULSE - Query the current pulse width
```
GET <channel> PULSE
```

Example:
```bash
echo "GET 0 PULSE" | nc -N -U /tmp/piservod.sock
# Response: PULSE 1500
```

#### GET STATE - Query the channel state
```
GET <channel> STATE
```

Example:
```bash
echo "GET 0 STATE" | nc -N -U /tmp/piservod.sock
# Response: GPIO 17 ENABLE 1
```

### Complete Example Session
```bash
# Connect to the daemon
nc -U /tmp/piservod.sock

# Setup servo on channel 0, GPIO pin 17
SETUP 0 GPIO 17
# OK

# Set custom range
SET 0 RANGE 500 2500
# OK

# Move to center position
SET 0 PULSE 1500
# OK

# Enable the servo
ENABLE 0
# OK

# Check current state
GET 0 STATE
# GPIO 17 ENABLE 1

# Move servo to different positions
SET 0 PULSE 500
# OK

SET 0 PULSE 2500
# OK

# Disable the servo
DISABLE 0
# OK
```

### Error Responses
If a command fails, the daemon returns:
```
ERROR <message>
```

Common errors:
- `ERROR Invalid command` - Malformed command syntax
- `ERROR Invalid channel` - Channel number out of range (0-7)
- `ERROR Invalid GPIO pin` - GPIO pin number invalid
- `ERROR Channel not configured` - Channel must be set up with SETUP first
- `ERROR Pulse value out of range` - Pulse value outside configured min/max range
- `ERROR Invalid range: min must be less than max` - Range validation failed

## Technical Details

### Architecture
- Supports up to 8 servo channels simultaneously
- PWM frame rate: 50Hz (20ms period)
- Default pulse range: 1000-2000μs
- Default neutral position: 1500μs
- Uses timerfd for accurate timing
- Real-time scheduling (SCHED_FIFO) for timing precision

### Software PWM vs Hardware PWM

**Why Software PWM?**
The Raspberry Pi has limited hardware PWM channels (typically 2), but many applications need to control multiple servos simultaneously. This daemon provides software-based PWM that can drive up to 8 servos using any GPIO pins.

**Advantages:**
- Control 8+ servos on any GPIO pins
- No hardware PWM channel conflicts
- Flexible pin assignment
- Simple protocol-based control

**Trade-offs:**
- Higher CPU usage than hardware PWM
- Timing depends on system load and kernel scheduling
- Potential jitter under heavy system load

### Performance Characteristics

**Timing Accuracy:**
- Frame timing controlled by `timerfd` with `CLOCK_MONOTONIC`
- Real-time scheduling (SCHED_FIFO) reduces scheduling latency
- Typical jitter: <50μs under normal system load
- Increased jitter (100-500μs) possible under high CPU load or with non-RT kernels

**Multi-Servo Timing:**
- Servos are sorted by pulse width and activated sequentially within each 20ms frame
- All servos start HIGH simultaneously
- Each servo is cleared LOW at its specific pulse width time
- Inter-servo timing is deterministic within a single frame
- Total pulse width spread across all servos must fit within ~2ms (typical servo range)

### Limitations
- Maximum 8 simultaneous servo channels
- Not suitable for precision applications requiring <10μs accuracy
- Performance degrades with heavy system load
- Requires root access for real-time scheduling and GPIO access

## Contribting
Contributions are welcome!
