~~~ txt
+3.3V (Microcontroller VCC)
         │
        [R_pull: 10 kΩ]
         │
         ├───> To Microcontroller ADC Pin (0V - 3.3V max)
         │
   [Sensor Loop Input]
         │
  [2EOL/NC Detector Resistor Network]
         │
        GND
~~~


~~~ txt
+3.3V (STM32 VDD / VREF+)
         │
        [R_pull: 3.3 kΩ]
         │
         ├───┬─> To STM32 ADC Pin (e.g., PA1 / ADC1_IN1)
         │   │
        [C] [D]  <- Protection Elements (See Step 1)
         │   │
   [Sensor Loop Input]
         │
  [Satel TOPAZ 2EOL Network]
         │
        GND (STM32 VSS)
~~~


### **Step-by-Step Implementation**

  * Hardware Protection: Add a 100 nF ceramic capacitor between the ADC pin and GND right next to the microcontroller. Security loops act like long antennas and gather electromagnetic interference; this capacitor filters out high-frequency noise and stabilizes your readings.
  * Software Calibration: Do not program hard coded exact voltage targets. Instead, create tolerance windows in your microcontroller firmware to account for minor fluctuations in resistor values or wire lengths.
  * Firmware Logic Example (Pseudo-code):


    ~~~ c
    int adc_val = analogRead(ADC_PIN);

    if (adc_val < 200) {
        // State: Tamper Short (Wire compromised)
    } else if (adc_val >= 200 && adc_val < 580) {
        // State: Normal / Standby
    } else if (adc_val >= 580 && adc_val < 2000) {
        // State: Alarm (Motion Detected)
    } else {
        // State: Tamper Cut (Wire severed)
    }
    ~~~