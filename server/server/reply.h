#ifndef REPLY_H
#define REPLY_H

#include <string.h>

#define S150 "150 Opening BINARY mode data connection.\r\n"
#define S200 "200 Type set to I.\r\n"
#define S200_PORT "200 PORT command successful.\r\n"
#define S215 "215 UNIX Type: L8\r\n"
#define S220 "220 FTP server ready.\r\n"
#define S221 "221 Goodbye.\r\n"
#define S226 "226 Transfer complete."
#define S227 "227 Entering Passive Mode (%s,%d,%d)\r\n"
#define S230 "230 Login successful.\r\n"
#define S331 "331 Please specify the password.\r\n"
#define S425 "425 Use PORT or PASV first.\r\n"
#define S426 "426 Connection failed.\r\n"
#define S451 "451 Failed to read file from disk.\r\n"
#define S500 "500 Unknown command.\r\n"
#define S503 "503 Login with USER first.\r\n"
#define S504 "504 Unknown parameter.\r\n"
#define S530 "530 Login incorrect.\r\n"
#define S530_2 "530 Can't change from guest user.\r\n"

#endif
