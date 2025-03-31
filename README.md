### Git Repository Structure:

- `main.c`: The central file orchestrating the program. It initializes peripherals, manages tasks, and integrates all other modules.
- `mppt.c`: Implements the Maximum Power Point Tracking (MPPT) algorithm, adjusting the duty cycle based on power and voltage feedback.
- `ina219.c`: Initializes and reads sensor data (voltage, current, power) from INA219 sensors.
- `https_client.c`: Handles HTTPS requests, posting sensor data to an external Firebase database.
- `interrupt.c`: Manages timer-based interrupts and sets flags at predefined intervals to trigger specific tasks.

### Workflow:

The primary workflow executed by `main.c` can be described as follows:

1. **Initialization Phase** (`app_main()` function):
   - Initializes the non-volatile storage (`nvs_flash`), network interfaces, and event loops.
   - Connects to a WiFi network and synchronizes time using SNTP protocol.
   - Initializes the I²C device and creates a FreeRTOS task named `task`.

2. **Main Task Execution** (`task` function):
   - Starts INA219 sensors (input and output) and initializes PWM for controlling a SEPIC converter.
   - Initializes interrupts via `init_interrupts()`.
   - Enters an infinite loop that executes every 10 ms, governed by `vTaskDelay(pdMS_TO_TICKS(10))`. This is the only delay used in code. It ensures the CPU periodically yields control, preventing the watchdog timer from triggering errors due to continuous CPU usage without interruption.
   - Within this loop:
     - Checks for `mppt_flag`. When set (every 25 ms by interrupt), it:
       - Resets the flag.
       - Fetches input/output sensor data via INA219 sensors.
       - Executes the MPPT algorithm (`mppt()`) to update the PWM duty cycle.
     - Checks for `print_flag`. When set (every 200 ms by interrupt), it:
       - Outputs sensor readings and duty cycle to the console.
     - Checks for `http_flag`. When set (every 50 s by interrupt), it:
       - Resets the flag.
       - Creates and dispatches separate tasks (`http_test_task`) for sending sensor data (power and voltage) to an external Firebase server via HTTPS.

3. **MPPT Execution** (`mppt()` function):
   - Evaluates current power (`P`) and voltage (`V`) against previous measurements to determine whether to increase or decrease the PWM duty cycle to track the maximum power point.

4. **Sensor Data Handling** (`ina219.c`):
   - Continuously provides calibrated voltage, current, and power measurements.
   - Uses the INA219 component library developed by Ruslan V. Uss, distributed under the BSD license.

5. **HTTPS Data Transmission** (`https_client.c`):
   - Formats sensor data into JSON and securely transmits it to Firebase, handling memory and connection events appropriately.

6. **Interrupt Handling** (`interrupt.c`):
   - Sets up three ESP timers:
     - `mppt_timer` fires every 25 ms, triggering MPPT calculations.
     - `print_timer` fires every 200 ms, triggering console outputs.
     - `http_timer` fires every 50 s, triggering HTTP data transmissions.
   - These timers set respective flags, initiating tasks within the main loop.

This workflow ensures continuous power optimization through MPPT, regular sensor data updates, and real-time remote data logging.

