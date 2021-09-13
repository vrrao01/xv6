// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void consputc(int);

static int panicked = 0;

static struct
{
  struct spinlock lock;
  int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if (sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do
  {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if (locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint *)(void *)(&fmt + 1);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
  {
    if (c != '%')
    {
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch (c)
    {
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if ((s = (char *)*argp++) == 0)
        s = "(null)";
      for (; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if (locking)
    release(&cons.lock);
}

void panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for (i = 0; i < 10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for (;;)
    ;
}

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define LEFT_ARROW 228
#define RIGHT_ARROW 229
#define UP_ARROW 226
#define DOWN_ARROW 227
#define CRTPORT 0x3d4
static ushort *crt = (ushort *)P2V(0xb8000); // CGA memory
int max_pos = 0;                             // Position of rightmost character
static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  if (c == '\n')
    pos += 80 - pos % 80;
  else if (c == BACKSPACE)
  {
    /*
    Ensure the cursor is not at the leftmost position and
    appropriate shift the characters to the left. 
    */
    if (pos > 0)
    {
      --pos;
      for (int t = pos; t < max_pos; t++)
        crt[t] = ((crt[t + 1] & 0xff) | 0x0700);
      crt[max_pos] = ' ' | 0x0700;
      --max_pos;
    }
  }

  else if (c == LEFT_ARROW)
  {
    --pos; // Move cursor to the left
  }
  else if (c == RIGHT_ARROW)
  {
    ++pos; // Move cursor to the right
  }
  else
  {
    /* 
    Print entered character onto the screen. 
    Also, shift right in case entered character is in the middle.
    */
    for (int t = max_pos; t >= pos; t--)
      crt[t + 1] = ((crt[t] & 0xff) | 0x0700);
    max_pos++;
    if (pos > max_pos)
      max_pos = pos;
    crt[pos++] = (c & 0xff) | 0x0700; // black on white
  }

  if (pos < 0 || pos > 25 * 80)
    panic("pos under/overflow");

  if ((pos / 80) >= 24)
  { // Scroll up.
    memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
    pos -= 80;
    memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
}

#define INPUT_BUF 128
#define MAX_HISTORY 16
struct
{
  char buf[INPUT_BUF];
  uint r; // Read index
  uint w; // Write index
  uint e; // Edit index
  uint m; // Rightmost character index
} input;

/*
Shell history ring structure
*/
struct
{
  char buffer[MAX_HISTORY][INPUT_BUF]; // 2D array to store commands
  int head;                            // Head of the queue
  int tail;                            // Tail of the queue
  int currentIndex;                    // Index of current command from history being viewed
  char partialBuffer[INPUT_BUF];       // Buffer to store command typed before accessing history
} historyRing;

void consputc(int c)
{
  if (panicked)
  {
    cli();
    for (;;)
      ;
  }

  if (c == RIGHT_ARROW)
  {
    uartputc(input.buf[input.e]);
    cgaputc(c);
    return;
  }

  if (c == LEFT_ARROW)
  {
    uartputc('\b');
    cgaputc(c);
    return;
  }
  if (c == BACKSPACE)
  {
    uartputc('\b');
    /*
    Shift all characters to the right of cursor, one position to the left.
    */
    for (uint t = input.e; t < input.m; t++)
    {
      uartputc(input.buf[t + 1]);
      input.buf[t] = input.buf[t + 1];
    }
    uartputc(' ');
    uartputc('\b');
    /*
    Move serial port cursor to correct edit position.
    */
    for (uint t = input.e; t < input.m; t++)
    {
      uartputc('\b');
    }
  }
  else
    uartputc(c);
  cgaputc(c);
}

/*
Removes the line of text currently display on the console.
*/
void clearConsoleLine()
{
  for (int i = input.e; i < input.m; i++)
  {
    consputc(RIGHT_ARROW); // Moves cursor to rightmost position
  }
  for (int i = input.r; i < input.m; i++)
  {
    consputc(BACKSPACE); // Clears text by repeated BACKSPACEs
  }
}

/*
Resets input buffer in order to fill command from history 
*/
void clearInputBufferLine()
{
  input.m = input.r;
  input.e = input.r;
}

/*
Copy command at index history.currentIndex into the input buffer
*/
void copyHistorytoInputBuffer()
{
  for (int i = 0; i < INPUT_BUF; i++)
  {
    if (historyRing.buffer[historyRing.currentIndex][i] == 0)
      break;
    input.buf[(input.r + i) % INPUT_BUF] = historyRing.buffer[historyRing.currentIndex][i];
    input.e++;
    input.m++;
  }
}

/*
Copies the command entered before pressing on up arrow into a temporary buffer
*/
void savePartialCommand()
{
  memset(historyRing.partialBuffer, 0, INPUT_BUF);
  for (int i = input.r; i < input.m; i++)
  {
    historyRing.partialBuffer[i - input.r] = input.buf[i % INPUT_BUF];
  }
}

/*
Copies the command typed before accessing history back into input buffer
*/
void copyPartialToInputBuffer()
{
  for (int i = 0; i < INPUT_BUF; i++)
  {
    if (historyRing.partialBuffer[i] == 0)
      break;
    input.buf[(input.r + i) % INPUT_BUF] = historyRing.partialBuffer[i];
    input.e++;
    input.m++;
  }
}

/*
Stores the entered command into the history shell ring.
*/
void saveHistory()
{
  if (historyRing.buffer[historyRing.head][0] == 0)
  {
    int length = input.m - input.r;
    for (int i = 0; i < length; i++)
    {
      historyRing.buffer[historyRing.tail][i] = input.buf[(input.r + i) % INPUT_BUF];
    }
  }
  else
  {
    historyRing.tail = (historyRing.tail + 1) % MAX_HISTORY;
    if (historyRing.tail == historyRing.head)
      historyRing.head = (historyRing.head + 1) % MAX_HISTORY;
    int length = input.m - input.r;
    memset(historyRing.buffer[historyRing.tail], 0, INPUT_BUF);
    for (int i = 0; i < length; i++)
    {
      historyRing.buffer[historyRing.tail][i] = input.buf[(input.r + i) % INPUT_BUF];
    }
  }
  historyRing.currentIndex = -1;
}

/*
Function used by sys_history system call to copy command at index = historyID into buffer
*/
int history(char *buffer, int historyID)
{
  if (historyID < 0 || historyID > 15)
    return 2;
  int index = (historyRing.head + historyID) % MAX_HISTORY;
  if (historyRing.buffer[index][0] == 0)
    return 1;
  memmove(buffer, historyRing.buffer[index], INPUT_BUF);
  return 0;
}
#define C(x) ((x) - '@') // Control-x

void consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0;
  // uint tempIndex;
  acquire(&cons.lock);
  while ((c = getc()) >= 0)
  {
    if (input.m < input.e)
      input.m = input.e;
    switch (c)
    {
    case C('P'): // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'): // Kill line.
      while (input.e != input.w &&
             input.buf[(input.e - 1) % INPUT_BUF] != '\n')
      {
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'):
    case '\x7f': // Backspace
      if (input.e != input.w)
      {
        input.m--;
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case LEFT_ARROW:
      if (input.e != input.w)
      {
        input.e--;
        consputc(c);
      }
      break;

    case RIGHT_ARROW:
      if (input.e != input.m)
      {
        consputc(c);
        input.e++;
      }
      break;

    case UP_ARROW:
      if (historyRing.buffer[historyRing.head][0] != 0)
      {
        clearConsoleLine();
        if (historyRing.currentIndex == -1)
        {
          historyRing.currentIndex = historyRing.tail;
          savePartialCommand();
        }
        else if (historyRing.currentIndex != historyRing.head)
          historyRing.currentIndex = (historyRing.currentIndex + MAX_HISTORY - 1) % MAX_HISTORY;
        clearInputBufferLine();
        release(&cons.lock); // Release console lock so that cprint can aquire lock
        cprintf(historyRing.buffer[historyRing.currentIndex]);
        acquire(&cons.lock);
        copyHistorytoInputBuffer();
      }
      break;
    case DOWN_ARROW:
      if (historyRing.buffer[historyRing.head][0] != 0)
      {
        if (historyRing.currentIndex == historyRing.tail)
        {
          historyRing.currentIndex = -1;
          clearConsoleLine();
          clearInputBufferLine();
          release(&cons.lock);
          cprintf(historyRing.partialBuffer);
          acquire(&cons.lock);
          copyPartialToInputBuffer();
        }
        else if (historyRing.currentIndex != -1)
        {
          historyRing.currentIndex = (historyRing.currentIndex + 1) % MAX_HISTORY;
          clearConsoleLine();
          clearInputBufferLine();
          copyHistorytoInputBuffer();
          release(&cons.lock);
          cprintf(historyRing.buffer[historyRing.currentIndex]);
          acquire(&cons.lock);
        }
      }
      break;
    default:
      if (c != 0 && input.e - input.r < INPUT_BUF)
      {
        c = (c == '\r') ? '\n' : c;
        if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          input.e = input.m; // Move edit index to rightmost index to execute command
        }
        if (input.e < input.m)
        {
          /*
          Shift right input buffer  in case character is not added at rightmost position.
          */
          int n = input.m - input.e;
          for (int i = n; i > 0; i--)
          {
            input.buf[(input.e + i) % INPUT_BUF] = input.buf[(input.e + i - 1) % INPUT_BUF];
          }
          input.m++;
        }
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF)
        {
          input.w = input.e;
          saveHistory();
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if (doprocdump)
  {
    procdump(); // now call procdump() wo. cons.lock held
  }
}

int consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while (n > 0)
  {
    while (input.r == input.w)
    {
      if (myproc()->killed)
      {
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if (c == C('D'))
    { // EOF
      if (n < target)
      {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if (c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for (i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}