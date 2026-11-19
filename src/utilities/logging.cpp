#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef TEST_LOGS

// ========== Static Variables ==========

// Static variables to track duplicate messages
static char last_message[256] = {0};
static int duplicate_count = 0;
static bool suppressing = false;

// ========== Public Interface Methods ==========

/**
 * @brief Test logging function with duplicate suppression
 *
 * This function tracks duplicate messages and suppresses excessive repetition:
 * - Suppresses duplicate messages until every 25th occurrence
 * - Shows count on every 25th duplicate
 * - Resets when a new message is encountered
 *
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 */
void log_t_impl(const char *format, ...)
{
    // Buffer for the formatted message
    char current_message[256];

    // Format the message
    va_list args;
    va_start(args, format);
    vsnprintf(current_message, sizeof(current_message), format, args);
    va_end(args);

    // Check if this is a duplicate of the last message
    if (strcmp(current_message, last_message) == 0)
    {
        duplicate_count++;

        // Print every 25th duplicate
        if (duplicate_count % 25 == 0)
        {
            printf("[T] %s (x%d)\n", current_message, duplicate_count + 1);
        }
        // Otherwise suppress the message
        suppressing = true;
    }
    else
    {
        // New message - reset tracking
        if (suppressing && duplicate_count > 0)
        {
            // If we were suppressing, show final count before new message
            if (duplicate_count % 25 != 0)
            {
                printf("[T] (total x%d)\n", duplicate_count + 1);
            }
        }

        // Print the new message
        printf("[T] %s\n", current_message);

        // Update tracking state
        strncpy(last_message, current_message, sizeof(last_message) - 1);
        last_message[sizeof(last_message) - 1] = '\0';
        duplicate_count = 0;
        suppressing = false;
    }
}

#endif // TEST_LOGS