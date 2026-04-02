//
// Created by AQin on 2022/11/29.
//
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include "re_printf.h"
#include <stdint.h>



#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

UART_HandleTypeDef *gHuart;

void RetargetInit(UART_HandleTypeDef *huart)
{
  gHuart = huart;

  /* Disable I/O buffering for STDOUT stream, so that
   * chars are sent out as soon as they are printed. */
  setvbuf(stdout, NULL, _IONBF, 0);
}

int _isatty(int fd)
{
  if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
    return 1;

  errno = EBADF;
  return 0;
}

int _write(int fd, char *ptr, int len)
{
  HAL_StatusTypeDef hstatus;

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
  {
    //hstatus = HAL_UART_Transmit(gHuart, (uint8_t *) ptr, len, HAL_MAX_DELAY);
    hstatus = HAL_UART_Transmit(gHuart, (uint8_t *) ptr, len, 100);

    // 如果发生错误或超时，清除一下串口状态，防止后续持续卡死
    if (hstatus != HAL_OK) {
      __HAL_UNLOCK(gHuart); // 强行解锁
      gHuart->gState = HAL_UART_STATE_READY;
    }

    return len; // 即使超时也返回len，防止 printf 内部报错退出
    // if (hstatus == HAL_OK)
    //   return len;
    // else
    //   return EIO;
  }
  errno = EBADF;
  return -1;
}

int _close(int fd)
{
  if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
    return 0;

  errno = EBADF;
  return -1;
}

int _lseek(int fd, int ptr, int dir)
{
  (void) fd;
  (void) ptr;
  (void) dir;

  errno = EBADF;
  return -1;
}

int _read(int fd, char *ptr, int len)
{
  HAL_StatusTypeDef hstatus;

  if (fd == STDIN_FILENO)
  {
    hstatus = HAL_UART_Receive(gHuart, (uint8_t *) ptr, 1, HAL_MAX_DELAY);
    if (hstatus == HAL_OK)
      return 1;
    else
      return EIO;
  }
  errno = EBADF;
  return -1;
}

int _fstat(int fd, struct stat *st)
{
  if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
  {
    st->st_mode = S_IFCHR;
    return 0;
  }

  errno = EBADF;
  return 0;
}
