/* sm99img --- read screen image from Fluke ScopeMeter 99   2024-04-25 */
/* Copyright (c) 2024 John Honniball, Froods Software Development      */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define MAXX (240)
#define MAXY (240)


int Fd = 0;
unsigned char FrameBuf[MAXY][MAXX / 8];


/* ClearBuffer --- clear the frame buffer */

static void ClearBuffer(void)
{
   memset(FrameBuf, 0, sizeof (FrameBuf));
}


/* SetBit --- set a single bit or pixel in the frame buffer */

static void SetBit(const int x, const int y)
{
   FrameBuf[y][x / 8] |= 0x80 >> (x % 8);
}


/* WritePBM --- write the frame buffer to a file in PBM binary format */

static int WritePBM(const char *filename)
{
   FILE *fp;
   
   fp = fopen(filename, "w+");
   
   fprintf(fp, "P4\n# CREATOR: sm99img 1.0\n%d %d\n", MAXX, MAXY);
   
   fwrite(FrameBuf, 1, sizeof(FrameBuf), fp);
   
   fclose(fp);
   
   return (0);
}


/* openSM99Port --- open serial port to ScopeMeter */

static int openSM99Port(const char *port)
{
   int fd;
   struct termios tbuf;
   long int fdflags;

   fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
   
   if (fd < 0) {
      perror(port);
      exit(1);
   }
   
   if ((fdflags = fcntl(fd, F_GETFL, NULL)) < 0) {
      perror("fcntl GETFL");
      exit(1);
   }
   
   fdflags &= ~O_NDELAY;
   
   if (fcntl(fd, F_SETFL, fdflags) < 0) {
      perror("fcntl SETFL");
      exit(1);
   }

   if (tcgetattr(fd, &tbuf) < 0) {
      perror("tcgetattr");
      exit(1);
   }
   
   cfsetospeed(&tbuf, B1200);
   cfsetispeed(&tbuf, B1200);
   cfmakeraw(&tbuf);
   
   tbuf.c_cflag |= CLOCAL;
   tbuf.c_cflag &= ~CSIZE;
   tbuf.c_cflag |= CS8 | CREAD;
   tbuf.c_cflag &= ~PARENB;
   tbuf.c_cflag &= ~CRTSCTS;
   
   tbuf.c_cc[VMIN] = 0;
   tbuf.c_cc[VTIME] = 10;
   
   if (tcsetattr(fd, TCSAFLUSH, &tbuf) < 0) {
      perror("tcsetattr");
      exit(1);
   }
   
   return (fd);
}


/* SM99Send --- send a command via serial to the ScopeMeter 99 */

void SM99Send(const char *str)
{
   const int n = strlen(str);
   
   if (write(Fd, str, n) != n)
      perror("write");
}


/* SM99ReadToCR --- receive a CR-terminated string from the ScopeMeter 99 */

int SM99ReadToCR(char *str)
{
   int i;
   char ch;
   char buf[2];
   
   i = 0;
   
   do {
//    printf("Before read()...\n");
      if (read(Fd, buf, 1) < 0) {
         perror("read");
         exit(1);
      }
      ch = buf[0];
//    printf("ch = %02x\n", ch);
      if (ch != '\r')
         str[i++] = ch;
   } while (ch != '\r');

   str[i] = '\0';
   
   return (i);
}


/* SM99ReadToComma --- read characters until we get a comma delimiter */

int SM99ReadToComma(char *str)
{
   int i;
   char ch;
   char buf[2];
   
   i = 0;
   
   do {
      if (read(Fd, buf, 1) < 0) {
         perror("read");
         exit(1);
      }
      ch = buf[0];

      if (ch != ',')
         str[i++] = ch;
   } while (ch != ',');

   str[i] = '\0';
   
   return (i);
}


/* SM99ReadFixedLen --- read a fixed-length block from the ScopeMeter 99 */

int SM99ReadFixedLen(unsigned char *bin, const int n)
{
   int i;
   int cksum;
   unsigned char ch;
   unsigned char buf[2];
   
   cksum = 0;
   
   for (i = 0; i < n; i++) {     /* Read main data block */
      if (read(Fd, buf, 1) < 0) {
         perror("read");
         exit(1);
      }
      ch = buf[0];
      bin[i] = ch;
      cksum += ch;
   }

   if (read(Fd, buf, 1) < 0) {   /* Read extra checksum byte */
      perror("read");
      exit(1);
   }

   cksum &= 0xff;
   
   if (cksum == buf[0])
      return (i);
   else {
      printf("Checksum error (expected %d, read %d)\n", cksum, buf[0]);
      return (-1);
   }
}


/* ConvertEpsonToBitmap --- parse Epson escape codes and generate bitmap */

int ConvertEpsonToBitmap(const unsigned char *epson, const int len)
{
   int i;
   int x, y;
   
   ClearBuffer();
   
   y = 0;
   
   for (i = 0; i < len; i++) {
      if (epson[i] == 0x1B) {
         int mode;
         int n1, n2;
         int nx;
         int y1;
         unsigned char bits;
         unsigned char mask;
   
         switch (epson[++i]) {
         case '@':             /* Reset printer */
            break;
         case '*':             /* General bit image command */
            mode = epson[++i];
            n1 = epson[++i];
            n2 = epson[++i];
            nx = (n2 * 256) + n1;

            for (x = 0; x < nx; x++) {
               bits = epson[++i];

               for (mask = 0x80, y1 = 0; y1 < 8; mask >>= 1, y1++) {
                  if (bits & mask)
                     SetBit(x, y + y1);
               }
            }
            
            y += 8;
            
            break;
         case 'A':             /* Select line spacing */
            i++;
            break;
         case 'M':             /* Select 12-pitch characters */
            break;
         case 'k':             /* Select font family */
            i++;
            break;
         default:
            break;
         }
      }
      /* else */
      /*    Plain text, ignore */
   }
   
   return (0);
}


/* Identify --- read ID string from scope and print it */

int Identify(void)
{
   char str[64];

   SM99Send("ID\r");
   
   SM99ReadToCR(str);

   printf("ID: ack = '%s'\n", str);

   if (str[0] == '0') {
      SM99ReadToCR(str);

      printf("ID: str = '%s'\n", str);
   }

   return (strlen(str));
}


/* InstrumentStatus --- read status byte from ScopeMeter 99 */

int InstrumentStatus(void)
{
   char str[64];
   int val;

   SM99Send("IS\r");
   
   SM99ReadToCR(str);

   if (str[0] == '0') {
      SM99ReadToCR(str);

      val = atoi(str);
      
      printf("IS: status = '%s' 0x%04x\n", str, val);
   }
   else {
      printf("IS: ack = '%s'\n", str);
   }

   return (strlen(str));
}


/* TestCV --- test unknown CV command */

int TestCV(void)
{
   char str[64];

   SM99Send("CV\r");
   
   SM99ReadToCR(str);

   printf("CV: ack = '%s'\n", str);

   if (str[0] == '0') {
      SM99ReadToCR(str);

      printf("CV: str = '%s'\n", str);
   }

   return (strlen(str));
}


/* QueryGraphics --- read screen bitmap, convert,  and write to file */

int QueryGraphics(const int mode, const char *filename)
{
   char cmd[32];
   char fname[32];
   char str[32];
   unsigned char bin[8192];
   int nBytes = 0;
   FILE *fp;

   snprintf(cmd, sizeof(cmd), "QG%d\r", mode);

   SM99Send(cmd);
   
   SM99ReadToCR(str);

   printf("QG: ack = '%s'\n", str);

   if (str[0] == '0') {
      SM99ReadToComma(str);
      nBytes = atoi(str);
      
      printf("str = '%s', %d\n", str, nBytes);

      SM99ReadFixedLen(bin, nBytes);
      
      ConvertEpsonToBitmap(bin, nBytes);
      
      WritePBM(filename);
      
      snprintf(fname, sizeof(fname), "QG%d.bin", mode);

      fp = fopen(fname, "w+");
      
      fwrite(bin, 1, nBytes, fp);
      
      fclose(fp);
   }
   
   return (nBytes);
}


int main(const int argc, char *const argv[])
{
   /* Open the serial port connection to the SM99 */
   Fd = openSM99Port("/dev/ttyUSB1");
   
   Identify();
   TestCV();
   InstrumentStatus();
   
   QueryGraphics(129, "QG129.pbm");
// QueryGraphics(  2, "QG2.pbm");
   
   close(Fd);
   
   return (EXIT_SUCCESS);
}
