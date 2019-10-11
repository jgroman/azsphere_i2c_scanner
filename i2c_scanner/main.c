#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "applibs_versions.h"
#include <applibs/log.h>
#include <applibs/i2c.h>

// Import project hardware abstraction
#include <hw/project_hardware.h>

// Uncomment following directives to enable scanning at given bus speed
#define ENABLE_SCAN_BUS_SPEED_100K
#define ENABLE_SCAN_BUS_SPEED_400K
#define ENABLE_SCAN_BUS_SPEED_1M

// I2C BUS timeout in milliseconds
#define I2C_BUS_TIMEOUT_MS      100

// Strings to show detection status. Must be two chars plus trailing space
#define STR_NO_DETECTION    ".. "
#define STR_DETECTION       "[] "

// Support functions.
static void TerminationHandler(int signalNumber);
static int InitPeripheralsAndHandlers(void);
static void ClosePeripheralsAndHandlers(void);

// File descriptors - initialized to invalid value
static int i2cFd = -1;

// Termination state
static volatile sig_atomic_t terminationRequired = false;

/// <summary>
///     Signal handler for termination requests. This handler must be 
///     async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
	// Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
	terminationRequired = true;
}


/// <summary>
///     Set up SIGTERM termination handler, initialize peripherals and 
///     set up event handlers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static int InitPeripheralsAndHandlers(void)
{
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = TerminationHandler;
	sigaction(SIGTERM, &action, NULL);


    return 0;
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
	close(i2cFd);
}

/// <summary>
///     Perform scan of I2C bus at given speed.
/// </summary>
static void PerformScan(uint32_t busSpeed)
{
    I2C_DeviceAddress devAddr;
    uint8_t reply;

    uint8_t scanResult[128];
    bool detectionSuccess;

    Log_Debug("---- I2C Scan at ");
    switch (busSpeed)
    {
    case I2C_BUS_SPEED_STANDARD:
        Log_Debug("100 kHz\n");
        break;

    case I2C_BUS_SPEED_FAST:
        Log_Debug("400 kHz\n");
        break;

    case I2C_BUS_SPEED_FAST_PLUS:
        Log_Debug("1 MHz\n");
        break;

    default:
        Log_Debug("unknown speed\n");
        break;
    }

    i2cFd = I2CMaster_Open(PROJECT_ISU2_I2C);
    if (i2cFd < 0) {
        Log_Debug("ERROR: I2CMaster_Open: errno=%d (%s)\n",
            errno, strerror(errno));
        return;
    }

    if (I2CMaster_SetBusSpeed(i2cFd, busSpeed) != 0) {
        Log_Debug("ERROR: Failed to set I2C bus speed: errno=%d (%s)\n",
            errno, strerror(errno));
        return;
    }

    if (I2CMaster_SetTimeout(i2cFd, I2C_BUS_TIMEOUT_MS) != 0) {
        Log_Debug("ERROR: I2CMaster_SetTimeout: errno=%d (%s)\n",
            errno, strerror(errno));
        return;
    }


    // Print top header
    Log_Debug("     ");
    for (uint8_t addrL = 0; addrL < 0x10; addrL++)
    {
        Log_Debug("0%01X ", addrL);
    }
    Log_Debug("\n");

    // Scan complete I2C address range
    for (uint8_t addrH = 0; addrH < 0x80; addrH = (uint8_t)(addrH + 0x10))
    {
        Log_Debug("0x%02X ", addrH);
        for (uint8_t addrL = 0; addrL < 0x10; addrL++)
        {
            devAddr = addrH | addrL;

            if (devAddr == 0) {
                Log_Debug("   ", addrL);
                scanResult[devAddr] = 0;
            }
            else {
                // 0-byte I2C reads are not supported on the MT3620.
                if (I2CMaster_Read(i2cFd, devAddr, &reply, 1) != -1) {
                    Log_Debug(STR_DETECTION);
                    scanResult[devAddr] = 1;
                }
                else {
                    Log_Debug(STR_NO_DETECTION);
                    scanResult[devAddr] = 0;
                }
            }
        }
        Log_Debug("\n");
    }

    // Print summary
    detectionSuccess = false;
    Log_Debug("\n *** I2C devices detected at: ");
    for (devAddr = 1; devAddr < 0x80; devAddr++)
    {
        if (scanResult[devAddr] != 0) {
            Log_Debug("0x%02X ", devAddr);
            detectionSuccess = true;
        }
    }

    if (!detectionSuccess) {
        Log_Debug("NO DEVICES DETECTED");
    }

    Log_Debug("\n\n");

    close(i2cFd);
}

/// <summary>
///     Main entry point for this application.
/// </summary>
int main(int argc, char *argv[])
{
	if (InitPeripheralsAndHandlers() != 0) {
		terminationRequired = true;
	}

	if (!terminationRequired) {
#ifdef ENABLE_SCAN_BUS_SPEED_1M
        PerformScan(I2C_BUS_SPEED_FAST_PLUS);
#endif

#ifdef ENABLE_SCAN_BUS_SPEED_400K
        PerformScan(I2C_BUS_SPEED_FAST);
#endif

#ifdef ENABLE_SCAN_BUS_SPEED_100K
        PerformScan(I2C_BUS_SPEED_STANDARD);
#endif
	}

	ClosePeripheralsAndHandlers();
	return 0;
}

