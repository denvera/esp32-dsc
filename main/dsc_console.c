#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

#include "config.h"
#include "dsc_console.h"
#include "keybus_handler.h"

static void register_monitor();
static void register_write();
static void register_reset();
static void register_version();

static int monitor_mode(int argc, char** argv);
static int keybus_write(int argc, char** argv);
static int reset(int argc, char** argv);
static int version(int argc, char** argv);

static struct {
    struct arg_int *write_str;
    struct arg_end *end;
} write_args;

void console_task(void *pvParameter) {
  esp_console_register_help_command();
  const char* prompt = LOG_COLOR_I "esp32-dsc> " LOG_RESET_COLOR;
  while(true) {
          /* Get a line using linenoise.
           * The line is returned when ENTER is pressed.
           */
          char* line = linenoise(prompt);
          if (line == NULL) { /* Ignore empty lines */
              continue;
          }
          /* Add the command to the history */
          linenoiseHistoryAdd(line);
          /* Try to run the command */
          int ret;
          esp_err_t err = esp_console_run(line, &ret);
          if (err == ESP_ERR_NOT_FOUND) {
              printf("Unrecognized command\n");
          } else if (err == ESP_ERR_INVALID_ARG) {
              // command was empty
          } else if (err == ESP_OK && ret != ESP_OK) {
              printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(err));
          } else if (err != ESP_OK) {
              printf("Internal error: %s\n", esp_err_to_name(err));
          }
          /* linenoise allocates line buffer on the heap, so need to free it */
          linenoiseFree(line);
      }
}

static void register_monitor() {
  const esp_console_cmd_t cmd = {
        .command = "mon",
        .help = "Enable monitor mode",
        .hint = NULL,
        .func = &monitor_mode,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int monitor_mode(int argc, char** argv) {
  toggle_monitor_mode();
  return 0;
}

static void register_write() {
  const esp_console_cmd_t cmd = {
        .command = "write",
        .help = "Write to KeyBus",
        .hint = NULL,
        .func = &keybus_write,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int keybus_write(int argc, char** argv)
{
  int k;
  char c;
  for (int i = 1; i < argc; i++) {
    if (sscanf(argv[i], "%i", &k) > 0) {
      printf("Writing %02X\n", k);
      c = k & 0xff;
      xQueueSend(write_queue, &c, NULL);
    }
  }
  return 0;
}

static void register_reset() {
  const esp_console_cmd_t cmd = {
        .command = "reset",
        .help = "Reboot ESP32 module",
        .hint = NULL,
        .func = &reset,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int reset(int argc, char** argv) {
   esp_restart();
}

static void register_version() {
  const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Print firmware version",
        .hint = NULL,
        .func = &version,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
static int version(int argc, char** argv) {
  printf("ESP32 DSC Gateway v%s by Denver Abrey [denver@bitfire.co.za]\n", VERSION);
  return 0;
}

void initialize_console()
{
    /* Disable buffering on stdin and stdout */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install(CONFIG_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_CONSOLE_UART_NUM);

    /* Initialize the console */
    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
#if CONFIG_LOG_COLORS
            .hint_color = atoi(LOG_COLOR_CYAN)
#endif
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif

  register_monitor();
  register_write();
  register_reset();
  register_version();
}
