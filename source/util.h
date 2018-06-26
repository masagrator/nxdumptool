#pragma once

#ifndef __UTIL_H__
#define __UTIL_H__

#include <switch.h>

#define APP_VERSION "1.0.4"
#define NAME_BUF_LEN 4096

bool isGameCardInserted(FsDeviceOperator* o);

void syncDisplay();

void delay(u8 seconds);

bool getGameCardTitleID(u64 *titleID);

bool getGameCardControlNacp(u64 titleID, char *nameBuf, int nameBufSize, char *authorBuf, int authorBufSize, char *versionBuf, int versionBufSize);

int getSdCardFreeSpace(u64 *out);

void convertSize(u64 size, char *out, int bufsize);

void getCurrentTimestamp(char *out, int bufsize);

void waitForButtonPress();

bool isDirectory(char *path);

void addString(char **filenames, int *filenamesCount, char **nextFilename, const char *string);

void getDirectoryContents(char *filenameBuffer, char **filenames, int *filenamesCount, const char *directory, bool skipParent);

bool gameCardDumpNSWDBCheck(u32 crc, char *releaseName, int bufsize);

char *RemoveIllegalCharacters(char *name);

#endif
