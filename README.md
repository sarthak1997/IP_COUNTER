# IP_COUNTER

 - Handles continuous coming packets with limited buffer size to store them, cleans up the expired IPs after a defined period, and generate report periodically.
 - Various parameters like size, expiry and display period can be configured by configuration file.
 - It is implemented in C++ with different threads for insert, cleanup and display, along with the use of mutex locks.
 - For efficiency, multiple threads work on allocated parts of buffer for insert operation.
