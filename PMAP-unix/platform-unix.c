#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <ctype.h>

#include "../base/platform.h"
#include "../base/mecha.h"

static int ComPortHandle = -1;
static unsigned short RxTimeout;
static FILE *DebugOutputFile = NULL;

int PlatOpenCOMPort(const char *device)
{
    struct termios options;
    int result;

    if (ComPortHandle == -1)
    {
        // List available serial devices
        PlatShowMessage("Available serial devices in /dev/:\n");
        DIR *dir;
        const struct dirent *entry;

        dir = opendir("/dev");
        if (dir != NULL)
        {
            while ((entry = readdir(dir)))
            {
#ifdef __APPLE__
                // macOS uses /dev/cu.*
                if (strncmp(entry->d_name, "cu.", 3) == 0)
#else
                // Linux uses /dev/ttyS*, /dev/ttyUSB*, and /dev/ttyACM*
                if (strncmp(entry->d_name, "ttyS", 4) == 0 ||
                    strncmp(entry->d_name, "ttyUSB", 6) == 0 ||
                    strncmp(entry->d_name, "ttyACM", 6) == 0)
#endif
                {
                    PlatShowMessage("/dev/%s\n", entry->d_name);
                }
            }
            closedir(dir);
        }

        PlatShowMessage("Opening COM port: %s\n", device);

        ComPortHandle = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

        if (ComPortHandle != -1)
        {
            PlatShowMessage("COM port opened successfully.\n");

            fcntl(ComPortHandle, F_SETFL, 0);
            if (tcgetattr(ComPortHandle, &options) == -1)
            {
                PlatShowMessage("Failed to get terminal attributes. Error code: %d\n", errno);
                close(ComPortHandle);
                ComPortHandle = -1;
                return errno;
            }

            cfsetispeed(&options, B57600);
            cfsetospeed(&options, B57600);
            options.c_cflag &= ~PARENB; // No parity
            options.c_cflag &= ~CSTOPB; // 1 stop bit
            options.c_cflag &= ~CSIZE;
            options.c_cflag |= CS8;                     // 8 data bits
            options.c_cflag &= ~CRTSCTS;                // No hardware flow control
            options.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
            options.c_lflag = 0;
            options.c_oflag = 0;

            if (tcsetattr(ComPortHandle, TCSANOW, &options) == -1)
            {
                PlatShowMessage("Failed to set terminal attributes. Error code: %d\n", errno);
                close(ComPortHandle);
                ComPortHandle = -1;
                return errno;
            }

            if (tcflush(ComPortHandle, TCIOFLUSH) == -1)
            {
                PlatShowMessage("Failed to flush terminal I/O. Error code: %d\n", errno);
                close(ComPortHandle);
                ComPortHandle = -1;
                return errno;
            }

            RxTimeout = MECHA_TASK_NORMAL_TO;

            PlatShowMessage("COM port configuration set.\n");
            result = 0;
        }
        else
        {
            result = errno;
            PlatShowMessage("Failed to open COM port. Error code: %d\n", result);
        }
    }
    else
    {
        PlatShowMessage("COM port is already open.\n");
        result = EMFILE;
    }

    return result;
}

int PlatReadCOMPort(char *data, int n, unsigned short timeout)
{
    int result;

    if (ComPortHandle == -1)
    {
        PlatShowMessage("COM port is not open.\n");
        return -1; // Return an error code indicating that the COM port is not open.
    }

    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(ComPortHandle, &readfds);

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    result     = select(ComPortHandle + 1, &readfds, NULL, NULL, &tv);

    if (result > 0)
    {
        // Data is available, read it
        result = read(ComPortHandle, data, n);

        if (result < 0)
        {
            PlatShowMessage("Read from COM port failed.\n");
        }
    }
    else if (result == 0)
    {
        // Timeout
        PlatShowMessage("Read from COM port timed out.\n");
    }
    else
    {
        // Error
        PlatShowMessage("Select function error.\n");
    }

    return result;
}

int PlatWriteCOMPort(const char *data)
{
    int result = write(ComPortHandle, data, strlen(data));
    tcdrain(ComPortHandle);

    if (result < 0)
    {
        PlatShowMessage("Write to COM port failed.\n");
    }

    return result;
}

void PlatCloseCOMPort(void)
{
    if (ComPortHandle != -1)
    {
        PlatShowMessage("Closing COM port...\n");
        close(ComPortHandle);
        ComPortHandle = -1;
        PlatShowMessage("COM port closed.\n");
    }
    else
    {
        PlatShowMessage("COM port is already closed.\n");
    }
}

void PlatSleep(unsigned short int msec)
{
    usleep((useconds_t)msec * 1000);
}

void PlatShowEMessage(const char *format, ...)
{
    if (format == NULL)
    {
        fprintf(stderr, "Error: format string is NULL\n");
        return; // Exit early if format is NULL
    }

    va_list args;

    // Print to standard output
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Print to debug output file, if specified
    if (DebugOutputFile != NULL)
    {
        va_start(args, format);
        vfprintf(DebugOutputFile, format, args);
        va_end(args);
    }
}

void PlatShowMessage(const char *format, ...)
{
    if (format == NULL)
    {
        fprintf(stderr, "Error: format string is NULL\n");
        return; // Exit early if format is NULL
    }

    va_list args;

    // Print to standard output
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Print to debug output file, if specified
    if (DebugOutputFile != NULL)
    {
        va_start(args, format);
        vfprintf(DebugOutputFile, format, args);
        va_end(args);
    }
}

void PlatShowMessageB(const char *format, ...)
{
    if (format == NULL)
    {
        fprintf(stderr, "Error: format string is NULL\n");
        return; // Exit early if format is NULL
    }

    va_list args;

    // Print to standard output
    va_start(args, format);
    vprintf(format, args);
    vprintf(format, args);
    va_end(args); // Clean up after using args for vprintf

    // Print to debug output file, if specified
    if (DebugOutputFile != NULL)
    {
        va_start(args, format); // Reinitialize args for vfprintf
        vfprintf(DebugOutputFile, format, args);
        va_end(args); // Clean up after using args for vfprintf
    }

    // Block until the user presses ENTER
    while (getchar() != '\n')
    {
        // Wait for newline character
    }
}

void PlatDebugInit(void)
{
    // Get the current time
    time_t rawtime;
    struct tm *timeinfo;
    char timestamp[20]; // Adjust the size according to your needs

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Format the timestamp (e.g., "2023-10-14_12-34-56")
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", timeinfo);

    // Create the filename with timestamp
    char filename[256]; // Adjust the size according to your needs
    snprintf(filename, sizeof(filename), "pmap_%s.log", timestamp);

    DebugOutputFile = fopen(filename, "w");
}

void PlatDebugDeinit(void)
{
    if (DebugOutputFile != NULL)
    {
        fclose(DebugOutputFile);
        DebugOutputFile = NULL;
    }
}

void PlatDPrintf(const char *format, ...)
{
    if (format == NULL)
    {
        fprintf(stderr, "Error: format string is NULL\n");
        return; // Exit early if format is NULL
    }

    va_list args;

    // Print to standard output
    va_start(args, format);
    if (DebugOutputFile != NULL)
        vfprintf(DebugOutputFile, format, args);
    va_end(args);
}

int pstricmp(const char *s1, const char *s2)
{
    char s1char, s2char;

    for (s1char = *s1, s2char = *s2; *s1 != '\0' && *s2 != '\0'; s1++, s2++, s1char = *s1, s2char = *s2)
    {
        if (isalpha(s1char))
            s1char = toupper(s1char);
        if (isalpha(s2char))
            s2char = toupper(s2char);
        if (s1char != s2char)
            break;
    }

    return (s1char - s2char);
}

int pstrincmp(const char *s1, const char *s2, int len)
{
    char s1char, s2char;

    for (s1char = *s1, s2char = *s2; *s1 != '\0' && *s2 != '\0' && len > 0; s1++, s2++, s1char = *s1, s2char = *s2, len--)
    {
        if (isalpha(s1char))
            s1char = toupper(s1char);
        if (isalpha(s2char))
            s2char = toupper(s2char);
        if (s1char != s2char)
            break;
    }

    return ((len == 0) ? 0 : s1char - s2char);
}
